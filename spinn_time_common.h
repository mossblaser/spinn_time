/**
 * Common definitions for time syncing experiment.
 */


#ifndef SPINN_TIME_COMMON_H
#define SPINN_TIME_COMMON_H

#include <spinnaker.h>

// Print diagnostic information
#define DEBUG_MASTER
//#define DEBUG_SLAVE

// Number of clock updates to perform before exiting (or zero to run forever)
#define NUM_CORRECTIONS 0

// Height of the (rectangular) system
#define WIDTH  12
#define HEIGHT 12

// The period in us over which to send out a single correction to each core (approx)
#define UPDATE_INTERVAL 10000000

// Should the packet generator produce packets with paylods?
#define GEN_USE_PAYLOAD FALSE

// The timer tick at which multicast packets are unleashed in packet generators
#define GEN_TIMER_TICK 100
#define GEN_TIMER_NOISE_RANGE 10

// Timer for master sending out requests (calculated from UPDATE_INTERVAL,
// result in us)
#define MASTER_TIMER_TICK (UPDATE_INTERVAL/(WIDTH*HEIGHT))

// Number of cores to use on each chip
#define CORES_PER_CHIP 16

// Clock divider setting to use (/1 = 0, /16 = 1, /256 = 2)
#define TC_DIVIDER 1

// Convert the divider code into the division magnitude
#define TC_DIVIDER_VAL ( (TC_DIVIDER == 0) ? 1 \
                       : (TC_DIVIDER == 1) ? 16 \
                       : (TC_DIVIDER == 2) ? 256 \
                       : 0 \
                       )

// Configuration bits for synchronised timers
#define TC_CONFIG ( (0 << 0)        /* Wrapping counter */ \
                  | (1 << 1)        /* 32-bit counter */ \
                  | (TC_DIVIDER<<2) /* Clock divider (/1 = 0, /16 = 1, /256 = 2) */ \
                  | (0 << 5)        /* No interrupt */ \
                  | (0 << 6)        /* Free-running */ \
                  | (1 << 7)        /* Enabled */ \
                  )


// Period at which the LED should be toggled (us)
#define LED_TOGGLE_PERIOD_US 100
// Covert the period into a number of clock ticks
#define LED_TOGGLE_PERIOD_TICKS (((sv->cpu_clk) * (LED_TOGGLE_PERIOD_US)) / TC_DIVIDER_VAL)

// Read the disciplined timer
#define TIMER_VALUE (-tc2[TC_COUNT])

// Defines a bit in the payload indicating a ping request
#define PL_PING_BIT (1<<31)

// Extract the signed correction from the payload
#define PL_TO_CORRECTION(pl) ((int)(((uint)(pl))|((((uint)pl))>>30)<<31))

#endif
