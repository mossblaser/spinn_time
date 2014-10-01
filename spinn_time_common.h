/**
 * Common definitions for time syncing experiment.
 */


#ifndef SPINN_TIME_COMMON_H
#define SPINN_TIME_COMMON_H

#include <spinnaker.h>

// Number of clock updates to perform before exiting
#define NUM_CORRECTIONS 0

// Height of the (rectangular) system
#define WIDTH  48
#define HEIGHT 24

// Number of cores to use on each chip
#define CORES_PER_CHIP 1

// Configuration bits for synchronised timers
#define TC_CONFIG ( (0 << 0) /* Wrapping counter */ \
	                | (1 << 1) /* 32-bit counter */ \
	                | (1 << 2) /* Clock divider (/1 = 0, /16 = 1, /256 = 2) */ \
	                | (0 << 5) /* No interrupt */ \
	                | (0 << 6) /* Free-running */ \
	                | (1 << 7) /* Enabled */ \
	                )


// Period at which the LED should be toggled
#define LED_TOGGLE_PERIOD (((sv->cpu_clk) * 500000) / 16)

#define TIMER_VALUE (-tc2[TC_COUNT])

// Defines a bit in the payload indicating a ping request
#define PL_PING_BIT (1<<31)

// Extract the signed correction from the payload
#define PL_TO_CORRECTION(pl) ((int)((pl)|((pl)>>30)<<31))

#endif
