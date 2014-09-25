#include <sark.h>
#include <spin1_api.h>

#include "dor.h"


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

void
setup_routing_tables(uint my_x, uint my_y, uint cores_per_chip)
{
	uint entry;
	
	uint xyz[3] = {
		XY_TO_MIN_X(my_x,my_y),
		XY_TO_MIN_Y(my_x,my_y),
		XY_TO_MIN_Z(my_x,my_y)
	};
	
	// Add entries to accept packets destined for this core
	for (int p = 1; p <= cores_per_chip; p++)
		ADD_RTR_ENTRY( XYZPD_TO_KEY(xyz[0],xyz[1],xyz[2], p-1,0)
		             , XYZPD_TO_KEY(  0xFF,  0xFF,  0xFF,0x0F,0)
		             , CORE(p)
		             );
	
	// Add entry to pick up return packets for the master
	if (my_x == 0 && my_y == 0)
		ADD_RTR_ENTRY( RETURN_KEY(XYZPD_TO_KEY(0,0,0,0,0))
		             , RETURN_MASK(XYZPD_TO_KEY(0,0,0,0,0))
		             , CORE(1)
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
				ADD_RTR_ENTRY( RETURN_KEY(XYZPD_TO_KEY(0,0,0,0,dim_order))
				             , RETURN_MASK(XYZPD_TO_KEY(0,0,0,0,0x7))
				             , OPPOSITE(DIM_DIRECTIONS[DIM_ORDERS[dim_order][2]])
				             );
			else if (xyz[DIM_ORDERS[dim_order][1]])
				ADD_RTR_ENTRY( RETURN_KEY(XYZPD_TO_KEY(0,0,0,0,dim_order))
				             , RETURN_MASK(XYZPD_TO_KEY(0,0,0,0,0x7))
				             , OPPOSITE(DIM_DIRECTIONS[DIM_ORDERS[dim_order][1]])
				             );
			else if (xyz[DIM_ORDERS[dim_order][0]])
				ADD_RTR_ENTRY( RETURN_KEY(XYZPD_TO_KEY(0,0,0,0,dim_order))
				             , RETURN_MASK(XYZPD_TO_KEY(0,0,0,0,0x7))
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

