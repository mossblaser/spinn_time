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
#define CORES_PER_CHIP 1

// Address in SDRAM to store results
#define RESULT_ADDR(x,y,p) ( ((uint*)SDRAM_BASE_BUF) \
                             + (((x) + ((y)*WIDTH)) * CORES_PER_CHIP) \
                             + (p)-1 \
                           )

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

// Routing key encode/decode
#define XYZP_TO_KEY(x,y,z,p) ( ((x)&0xFF)<<24 | ((y)&0xFF)<<16 | ((z)&0xFF)<<8 | ((p)&0xFF)<<0 )
#define KEY_TO_X(k) ( ((k) >> 24) & 0xFF )
#define KEY_TO_Y(k) ( ((k) >> 16) & 0xFF )
#define KEY_TO_Z(k) ( ((k) >>  8) & 0xFF )
#define KEY_TO_P(k) ( ((k) >>  0) & 0xFF )

// X/Y/Z to minimal X/Y/Z conversion
#define XYZ_TO_MIN_X(x,y,z) ((x) - MEDIAN((x),(y),(z)))
#define XYZ_TO_MIN_Y(x,y,z) ((y) - MEDIAN((x),(y),(z)))
#define XYZ_TO_MIN_Z(x,y,z) ((z) - MEDIAN((x),(y),(z)))

// (Positive-only) X/Y to minimal X/Y/Z conversion
#define XY_TO_MIN_X(x,y) ((x) - MIN((x),(y)))
#define XY_TO_MIN_Y(x,y) ((y) - MIN((x),(y)))
#define XY_TO_MIN_Z(x,y) (-MIN((x),(y)))

#define XYP_TO_KEY(x,y,p) (XYZP_TO_KEY( XY_TO_MIN_X((x),(y)) \
                                      , XY_TO_MIN_Y((x),(y)) \
                                      , XY_TO_MIN_Z((x),(y)) \
                                      , p \
                                      ))

// The position of this chip in the system
uint my_x = -1;
uint my_y = -1;
uint my_p = -1;

// Last destination sent to (on master)
uint dest_x = 0;
uint dest_y = 0;
uint dest_p = 1;

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

/**
 * Initialise the routing tables on this chip with a naive 0,0 to any, any to
 * 0,0 routing scheme.
 */
void
setup_routing_tables(void)
{
	uint entry;
	
	uint x = XY_TO_MIN_X(my_x,my_y);
	uint y = XY_TO_MIN_Y(my_x,my_y);
	uint z = XY_TO_MIN_Z(my_x,my_y);
	
	// Add entries to accept packets destined for this core
	for (int p = 1; p <= CORES_PER_CHIP; p++)
		ADD_RTR_ENTRY( XYZP_TO_KEY(x,y,z,p)
		             , XYZP_TO_KEY(0xFF,0xFF,0xFF,0xFF)
		             , CORE(p)
		             );
	
	if (my_x == 0 && my_y == 0) {
		// Start packets off on the right dimension from the master
		ADD_RTR_ENTRY( XYZP_TO_KEY(   0,   0,   0,   0)
		             , XYZP_TO_KEY(0xFF,0xFF,0x00,0x00)
		             , NORTH_EAST
		             );
		ADD_RTR_ENTRY( XYZP_TO_KEY(   0,   0,   0,   0)
		             , XYZP_TO_KEY(0xFF,0x00,0x00,0x00)
		             , NORTH
		             );
		ADD_RTR_ENTRY( XYZP_TO_KEY(   0,   0,   0,   0)
		             , XYZP_TO_KEY(0x00,0x00,0x00,0x00)
		             , EAST
		             );
	} else {
		// Send packets in the right direction to the master
		if (z)
			ADD_RTR_ENTRY( XYZP_TO_KEY(   0,   0,   0,   1)
			             , XYZP_TO_KEY(0xFF,0xFF,0xFF,0xFF)
			             , SOUTH_WEST
			             );
		else if (y)
			ADD_RTR_ENTRY( XYZP_TO_KEY(   0,   0,   0,   1)
			             , XYZP_TO_KEY(0xFF,0xFF,0xFF,0xFF)
			             , SOUTH
			             );
		else if (x)
			ADD_RTR_ENTRY( XYZP_TO_KEY(   0,   0,   0,   1)
			             , XYZP_TO_KEY(0xFF,0xFF,0xFF,0xFF)
			             , WEST
			             );
	}
	
	// Dimension order route packets (x, then y, then z)
	ADD_RTR_ENTRY( XYZP_TO_KEY(   x,   y,   0,   0)
	             , XYZP_TO_KEY(0xFF,0xFF,0x00,0x00)
	             , NORTH_EAST
	             );
	ADD_RTR_ENTRY( XYZP_TO_KEY(   x,   0,   0,   0)
	             , XYZP_TO_KEY(0xFF,0x00,0x00,0x00)
	             , NORTH
	             );
}


// Packet callback on slave
void
bounce_mc_packet(uint key, uint _1)
{
	spin1_send_mc_packet(XYP_TO_KEY(0,0,1), XYP_TO_KEY(my_x,my_y,my_p), 1);
	//io_printf(IO_BUF, "Returning bounce from %08x\n", key);
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
		*(RESULT_ADDR(dest_x,dest_y,dest_p)) = -(recv_time-send_time);
	else
		*(RESULT_ADDR(dest_x,dest_y,dest_p)) = 0;
	
	// Advance through the system
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
	
	// Stop after sending once to everyone
	if (dest_x == my_x && dest_y == my_y && dest_p == my_p) {
		spin1_exit(0);
	}
	
	// Send an empty packet to the remote to ping back
	uint key = XYP_TO_KEY(dest_x,dest_y,dest_p);
	got_reply = 0;
	send_time = tc2[TC_COUNT];
	spin1_send_mc_packet(key, 0, 1);
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
		spin1_callback_on(MCPL_PACKET_RECEIVED, on_master_mc_packet, 1);
		
		// Set up fine-grained timer for latency measurement
		tc2[TC_CONTROL] = (0 << 0) // Wrapping counter
		                | (1 << 1) // 32-bit counter
		                | (0 << 2) // Clock divider (/1 = 0, /16 = 1, /256 = 2)
		                | (0 << 5) // No interrupt
		                | (0 << 6) // Free-running
		                | (1 << 7) // Enabled
		                ;
	} else {
		spin1_callback_on(MCPL_PACKET_RECEIVED, bounce_mc_packet, 1);
	}
	
	spin1_start(TRUE);
}
