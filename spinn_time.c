/**
 * SpiNNaker application to test clock synchronisation on large systems.
 */

#include <sark.h>
#include <spin1_api.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)<(b))?(b):(a))
#define MEDIAN(a,b,c) MAX(MIN((a),(b)), MIN(MAX((a),(b)),(c)))

// Height of the (rectangular) system
#define WIDTH  96
#define HEIGHT 60

// Number of cores to use on each chip
#define CORES_PER_CHIP 16

// Include a payload
#define USE_PAYLOAD 1

// Router wait periods
#define WAIT1 0x00
#define WAIT2 0x00

// Timer for master sending out requests (us)
#define MASTER_TIMER_TICK 1000

// Get "route" bits
#define CORE(core) (1 << ((core) + 6))
#define EAST       (1<<0)
#define NORTH_EAST (1<<1)
#define NORTH      (1<<2)
#define WEST       (1<<3)
#define SOUTH_WEST (1<<4)
#define SOUTH      (1<<5)

// Get opposite direction for route
#define OPPOSITE(d) (((d)<<3 | (d)>>3) & 0x3F)

// Alternative dimension ordering for dimension order routing
#define NUM_DIM_ORDERS 6
#define DIM_ORDER_XYZ 0
#define DIM_ORDER_XZY 1
#define DIM_ORDER_YXZ 2
#define DIM_ORDER_YZX 3
#define DIM_ORDER_ZXY 4
#define DIM_ORDER_ZYX 5
const uint DIM_ORDERS[6][3] = {
	{0, 1, 2},
	{0, 2, 1},
	{1, 0, 2},
	{1, 2, 0},
	{2, 0, 1},
	{2, 1, 0}
};

// Direction of each dimension
const uint DIM_DIRECTIONS[3] = {
	EAST,
	NORTH,
	NORTH_EAST // Note this is technically the opposite the dimension's direction
};

// Address in SDRAM to store results
#define RESULT_ADDR(x,y,p,d) ( ((uint*)SDRAM_BASE_BUF) \
                               + (( (((x) + ((y)*WIDTH)) * CORES_PER_CHIP) \
                                  + (p)-1) * NUM_DIM_ORDERS) \
                               + (d) \
                             )

// Routing key encode/decode
#define XYZPD_TO_KEY(x,y,z,p,d) ( ((x)&0xFF)<<24 | ((y)&0xFF)<<16 | ((z)&0xFF)<<8 | ((p)&0x1F)<<3 | ((d)&0x07) )
#define KEY_TO_X(k) ( ((k) >> 24) & 0xFF )
#define KEY_TO_Y(k) ( ((k) >> 16) & 0xFF )
#define KEY_TO_Z(k) ( ((k) >>  8) & 0xFF )
#define KEY_TO_P(k) ( ((k) >>  3) & 0x1F )
#define KEY_TO_D(k) ( ((k) >>  0) & 0x07 )

// X/Y/Z to minimal X/Y/Z conversion
#define XYZ_TO_MIN_X(x,y,z) ((x) - MEDIAN((x),(y),(z)))
#define XYZ_TO_MIN_Y(x,y,z) ((y) - MEDIAN((x),(y),(z)))
#define XYZ_TO_MIN_Z(x,y,z) ((z) - MEDIAN((x),(y),(z)))

// (Positive-only) X/Y to minimal X/Y/Z conversion
#define XY_TO_MIN_X(x,y) ((x) - MIN((x),(y)))
#define XY_TO_MIN_Y(x,y) ((y) - MIN((x),(y)))
#define XY_TO_MIN_Z(x,y) (-MIN((x),(y)))

#define XYPD_TO_KEY(x,y,p,d) (XYZPD_TO_KEY( XY_TO_MIN_X((x),(y)) \
                                          , XY_TO_MIN_Y((x),(y)) \
                                          , XY_TO_MIN_Z((x),(y)) \
                                          , (p) \
                                          , (d) \
                                          ))

// The position of this chip in the system
uint my_x = -1;
uint my_y = -1;
uint my_p = -1;

// Last destination sent to (on master)
uint dest_x = 0;
uint dest_y = 0;
uint dest_p = 1;
uint dest_dim_order = 0;

// Got reply flag
volatile uint got_reply = FALSE;

// Packet send time
volatile uint send_time;
volatile uint recv_time;

// Utility macro for setup_routing_tables
#define ADD_RTR_ENTRY(key,mask,route) \
	do { \
		uint entry = rtr_alloc(1); \
		if (!entry) { \
			io_printf(IO_BUF, "Failed to allocate routing entry!\n");\
			continue; \
		} \
		uint success = rtr_mc_set(entry, key, mask, route); \
		if (!success) { \
			io_printf(IO_BUF, "Failed to set routing entry %d with key %08x mask %08x route %08x!\n", \
				entry,key,mask,route\
			);\
		} \
	} while (0)


// Macros which expand to 0xFF if the given dimension (defined by dim_order) is
// at that index or lower and zero otherwise. i.e. 0xFF if this dimension has
// been used by the given index.
#define DX0F (((DIM_ORDERS[dim_order][0]==0) ? 0xFF : 0x00)       )
#define DX1F (((DIM_ORDERS[dim_order][1]==0) ? 0xFF : 0x00) | DX0F)
#define DX2F (((DIM_ORDERS[dim_order][2]==0) ? 0xFF : 0x00) | DX1F)
#define DY0F (((DIM_ORDERS[dim_order][0]==1) ? 0xFF : 0x00)       )
#define DY1F (((DIM_ORDERS[dim_order][1]==1) ? 0xFF : 0x00) | DY0F)
#define DY2F (((DIM_ORDERS[dim_order][2]==1) ? 0xFF : 0x00) | DY1F)
#define DZ0F (((DIM_ORDERS[dim_order][0]==2) ? 0xFF : 0x00)       )
#define DZ1F (((DIM_ORDERS[dim_order][1]==2) ? 0xFF : 0x00) | DZ0F)
#define DZ2F (((DIM_ORDERS[dim_order][2]==2) ? 0xFF : 0x00) | DZ1F)

/**
 * Initialise the routing tables on this chip with a naive 0,0 to any, any to
 * 0,0 routing scheme. Also set up the timeouts.
 */
void
setup_routing_tables(void)
{
	// Set router timeout
	volatile uint *control_reg = (uint*)(RTR_BASE+RTR_CONTROL);
	uint control = *control_reg;
	control &= 0xFFFF;
	control |= (WAIT2<<24) | (WAIT1<<16);
	*control_reg = control;
	
	uint entry;
	
	uint xyz[3] = {
		XY_TO_MIN_X(my_x,my_y),
		XY_TO_MIN_Y(my_x,my_y),
		XY_TO_MIN_Z(my_x,my_y)
	};
	
	// Add entries to accept packets destined for this core
	for (int p = 1; p <= CORES_PER_CHIP; p++)
		ADD_RTR_ENTRY( XYZPD_TO_KEY(xyz[0],xyz[1],xyz[2],   p,0)
		             , XYZPD_TO_KEY(  0xFF,  0xFF,  0xFF,0x1F,0)
		             , CORE(p)
		             );
	
	// Add routes which allow dimension-order routing with any dimension order
	for (int dim_order = 0; dim_order < NUM_DIM_ORDERS; dim_order++) {
		if (my_x == 0 && my_y == 0) {
			// Start packets off on the right dimension from the master
			// If the first/second dimensions are zero, route in the third dimension
			ADD_RTR_ENTRY( XYZPD_TO_KEY(   0,   0,   0,   0, dim_order)
			             , XYZPD_TO_KEY(DX1F,DY1F,DZ1F,0x00, 0x07)
			             , DIM_DIRECTIONS[DIM_ORDERS[dim_order][2]]
			             );
			// If the first dimension is zero, route in the second dimension
			ADD_RTR_ENTRY( XYZPD_TO_KEY(   0,   0,   0,   0, dim_order)
			             , XYZPD_TO_KEY(DX0F,DY0F,DZ0F,0x00, 0x07)
			             , DIM_DIRECTIONS[DIM_ORDERS[dim_order][1]]
			             );
			// If no dimension is zero, route in the first dimension
			ADD_RTR_ENTRY( XYZPD_TO_KEY(   0,   0,   0,   0, dim_order)
			             , XYZPD_TO_KEY(0x00,0x00,0x00,0x00, 0x07)
			             , DIM_DIRECTIONS[DIM_ORDERS[dim_order][0]]
			             );
		} else {
			// Send packets in the opposite dimension order when returning to the
			// master.
			// If the third dimension is not zero, route along it first
			if (xyz[DIM_ORDERS[dim_order][2]])
				ADD_RTR_ENTRY( XYZPD_TO_KEY(   0,   0,   0,   1, dim_order)
				             , XYZPD_TO_KEY(0xFF,0xFF,0xFF,0x1F, 0x07)
				             , OPPOSITE(DIM_DIRECTIONS[DIM_ORDERS[dim_order][2]])
				             );
			else if (xyz[DIM_ORDERS[dim_order][1]])
				ADD_RTR_ENTRY( XYZPD_TO_KEY(   0,   0,   0,   1, dim_order)
				             , XYZPD_TO_KEY(0xFF,0xFF,0xFF,0x1F, 0x07)
				             , OPPOSITE(DIM_DIRECTIONS[DIM_ORDERS[dim_order][1]])
				             );
			else if (xyz[DIM_ORDERS[dim_order][0]])
				ADD_RTR_ENTRY( XYZPD_TO_KEY(   0,   0,   0,   1, dim_order)
				             , XYZPD_TO_KEY(0xFF,0xFF,0xFF,0x1F, 0x07)
				             , OPPOSITE(DIM_DIRECTIONS[DIM_ORDERS[dim_order][0]])
				             );
		}
		
		// Dimension order route packets (x, then y, then z)
		ADD_RTR_ENTRY( XYZPD_TO_KEY(xyz[0]&DX1F, xyz[1]&DY1F,xyz[2]&DZ1F,   0,dim_order)
		             , XYZPD_TO_KEY(       DX1F,        DY1F,       DZ1F,0x00,0x07)
		             , DIM_DIRECTIONS[DIM_ORDERS[dim_order][2]]
		             );
		ADD_RTR_ENTRY( XYZPD_TO_KEY(xyz[0]&DX0F, xyz[1]&DY0F,xyz[2]&DZ0F,   0,dim_order)
		             , XYZPD_TO_KEY(       DX0F,        DY0F,       DZ0F,0x00,0x07)
		             , DIM_DIRECTIONS[DIM_ORDERS[dim_order][1]]
		             );
	}
}


// Packet callback on slave
void
bounce_mc_packet(uint key, uint _1)
{
	spin1_send_mc_packet(XYPD_TO_KEY(0,0,1,KEY_TO_D(key)), 0, USE_PAYLOAD);
	io_printf(IO_BUF, "Returning bounce from %08x\n", key);
}


// Packet callback on master
void
on_master_mc_packet(uint key, uint remote_key)
{
	recv_time = tc2[TC_COUNT];
	got_reply = 1;
}


// Timer callback on master
void
on_tick(uint _1, uint _2)
{
	// Store results
	if (got_reply)
		*(RESULT_ADDR(dest_x,dest_y,dest_p,dest_dim_order)) = -(recv_time-send_time);
	else
		*(RESULT_ADDR(dest_x,dest_y,dest_p,dest_dim_order)) = 0;
	
	// Advance through the system
	if (++dest_dim_order >= NUM_DIM_ORDERS) {
		dest_dim_order = 0;
		if (++dest_p > CORES_PER_CHIP) {
			dest_p = 1;
			if (++dest_x >= WIDTH) {
				dest_x = 0;
				if (++dest_y >= HEIGHT) {
					dest_y = 0;
				}
				io_printf(IO_BUF, "Up to %d %d %d.\n", dest_x, dest_y, dest_p);
			}
		}
	}
	
	// Stop after sending once to everyone
	if (dest_x == my_x && dest_y == my_y && dest_p == my_p && dest_dim_order == 0) {
		spin1_exit(0);
	}
	
	// Send an empty packet to the remote to ping back
	uint key = XYPD_TO_KEY(dest_x,dest_y,dest_p, dest_dim_order);
	got_reply = 0;
	spin1_send_mc_packet(key, 0, USE_PAYLOAD);
	send_time = tc2[TC_COUNT];
}


void
c_main() {
	// Discover this core's position in the system
	uint chip_id = spin1_get_chip_id();
	my_x = (chip_id >> 8) & 0xFF;
	my_y = chip_id & 0xFF;
	my_p = spin1_get_core_id();
	
	io_printf(IO_BUF, "Starting spinn_time as %d %d %d...\n", my_x, my_y, my_p);
	
	if (leadAp)
		setup_routing_tables();
	
	// Set up callbacks
	if (my_x == 0 && my_y == 0 && my_p == 1) {
		spin1_set_timer_tick(MASTER_TIMER_TICK);
		spin1_callback_on(TIMER_TICK, on_tick, 0);
		if (USE_PAYLOAD)
			spin1_callback_on(MCPL_PACKET_RECEIVED, on_master_mc_packet, 1);
		else
			spin1_callback_on(MC_PACKET_RECEIVED, on_master_mc_packet, 1);
		
		// Set up fine-grained timer for latency measurement
		tc2[TC_CONTROL] = (0 << 0) // Wrapping counter
		                | (1 << 1) // 32-bit counter
		                | (0 << 2) // Clock divider (/1 = 0, /16 = 1, /256 = 2)
		                | (0 << 5) // No interrupt
		                | (0 << 6) // Free-running
		                | (1 << 7) // Enabled
		                ;
	} else {
		if (USE_PAYLOAD)
			spin1_callback_on(MCPL_PACKET_RECEIVED, bounce_mc_packet, 1);
		else
			spin1_callback_on(MC_PACKET_RECEIVED, bounce_mc_packet, 1);
	}
	spin1_start(TRUE);
}
