/**
 * Common definitions for time syncing experiment.
 */


#ifndef SPINN_TIME_COMMON_H
#define SPINN_TIME_COMMON_H


// Number of clock updates to perform before exiting
#define NUM_CORRECTIONS 1000

// Height of the (rectangular) system
#define WIDTH  30
#define HEIGHT 8

// Number of cores to use on each chip
#define CORES_PER_CHIP 1

// Configuration bits for timers
#define TC_CONFIG ( (0 << 0) /* Wrapping counter */ \
	                | (1 << 1) /* 32-bit counter */ \
	                | (1 << 2) /* Clock divider (/1 = 0, /16 = 1, /256 = 2) */ \
	                | (0 << 5) /* No interrupt */ \
	                | (0 << 6) /* Free-running */ \
	                | (1 << 7) /* Enabled */ \
	                )


// Defines a bit in the payload indicating a ping request
#define PL_PING_BIT (1<<31)

// Extract the signed correction from the payload
#define PL_TO_CORRECTION(pl) ((int)((pl)|((pl)>>30)<<31))

#endif
