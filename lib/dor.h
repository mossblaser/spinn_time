/**
 * A library of functions for setting up dimension-order routes in SpiNNaker.
 * Torus links are not used.
 */

#ifndef DOR_H
#define DOR_H

// Minimum of two values
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

// Maximum of two values
#ifndef MAX
#define MAX(a,b) (((a)<(b))?(b):(a))
#endif

// Median of three values
#ifndef MEDIAN
#define MEDIAN(a,b,c) MAX(MIN((a),(b)), MIN(MAX((a),(b)),(c)))
#endif

// Given a 3D hexagonal coordinate, get the minimised (shortest path) version
#define XYZ_TO_MIN_X(x,y,z) ((x) - MEDIAN((x),(y),(z)))
#define XYZ_TO_MIN_Y(x,y,z) ((y) - MEDIAN((x),(y),(z)))
#define XYZ_TO_MIN_Z(x,y,z) ((z) - MEDIAN((x),(y),(z)))

// Given an (all positive) 2D hexagonal coordinate, get the minimal hexagonal 3D
// coordinate.
#define XY_TO_MIN_X(x,y) ((x) - MIN((x),(y)))
#define XY_TO_MIN_Y(x,y) ((y) - MIN((x),(y)))
#define XY_TO_MIN_Z(x,y) (-MIN((x),(y)))


// Route bits for routing entries
#define EAST       (1<<0)
#define NORTH_EAST (1<<1)
#define NORTH      (1<<2)
#define WEST       (1<<3)
#define SOUTH_WEST (1<<4)
#define SOUTH      (1<<5)
#define CORE(core) (1 << ((core) + 6))

// Get opposite direction for a (non-core) route
#define OPPOSITE(d) (((d)<<3 | (d)>>3) & 0x3F)

// Alternative dimension orderings for dimension order routing
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

// Lookup for direction of each dimension
const uint DIM_DIRECTIONS[3] = {
	EAST,
	NORTH,
	NORTH_EAST // Note this is technically the opposite the dimension's direction
};

// Encode/decode the the fields of a routing key. The key is a bit field
// containing a 3D hexagonal coordinate (x,y,z), a core number (p) and a
// selected dimension ordering (d), 0-5. Note that keys only exist for cores
// 0-15.  The final bit of the key is a "return" bit. If one, the key routes
// from 0,0,0,1 to the specified core, if the bit is zero, the key routes from
// the specified core to 0,0,0,1.
#define XYZPD_TO_KEY(x,y,z,p,d) ( ((x)&0xFF)<<24 | ((y)&0xFF)<<16 | ((z)&0xFF)<<8 | ((p)&0xF)<<4 | ((d)&0x07)<<1 | 1<<0)
#define KEY_TO_X(k) (((k) >> 24) & 0xFF)
#define KEY_TO_Y(k) (((k) >> 16) & 0xFF)
#define KEY_TO_Z(k) (((k) >>  8) & 0xFF)
#define KEY_TO_P(k) (((k) >>  4) & 0x0F)
#define KEY_TO_D(k) (((k) >>  1) & 0x07)

#define XYPD_TO_KEY(x,y,p,d) (XYZPD_TO_KEY( XY_TO_MIN_X((x),(y)) \
                                          , XY_TO_MIN_Y((x),(y)) \
                                          , XY_TO_MIN_Z((x),(y)) \
                                          , (p) \
                                          , (d) \
                                          ))

// A key which broadcasts to the same core in neighbouring chips
#define NEAREST_NEIGHBOUR_KEY(colour, p) (XYZPD_TO_KEY((colour),0,0,(p),7))

// Convert the chip's X & Y coordinates into a colour
#define XY_TO_COLOUR(x,y) (((x)+(y))%3)

// Convert a key into the "return" version which routes back to 0,0,0,1
#define RETURN_KEY(k) (k & ~0x1)
#define RETURN_MASK(k) (k | 0x1)


/**
 * Initialise the routing tables on this chip with a naive 0,0 to any, any to
 * 0,0 routing scheme. If a routing table entry cannot be allocated using
 * rtr_alloc, prints a warning in IO_BUF and otherwise continues blindly.
 */
void setup_routing_tables(uint my_x, uint my_y, uint cores_per_chip);



#endif
