/**
 * A simple, standalone C test bench for the clock discipline algorithm.
 */

#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "disciplined_clock.h"


#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)<(b)) ? (b) : (a))
#endif


#define TWO_PI 6.2831853071795864769252866

// Terminate after this time has elapsed
#define SIM_DURATION 100

// Probability of printing a sample of the clock error
#define SAMPLE_PROB 0.00001

// Clock interval of the master and slave
#define MASTER_TICK_PERIOD ((1.0/200000000.0)*16.0)
#define SLAVE_TICK_PERIOD  ((1.0/200000000.0)*16.0*(1.0-(30.0/1000000.0)))

// Sinusoidal clock wandering
#define SLAVE_WANDER_PERIOD    (7.0*60.0)
#define SLAVE_WANDER_MAGNITUDE ((1.0/200000000.0)*16.0*(30.0/1000000.0))

// Period of the master clock at which corrections are sent
#define POLL_PERIOD ((dclk_time_t)((5.76 / ((1.0/200000000.0)*16.0))/48))

// Jitter standard-deviation added to correction values
#define JITTER_SD 3.0

dclk_state_t dclk;

// The "real" master and slave clocks.
dclk_time_t master_time = 0;
dclk_time_t slave_time  = 0;


dclk_time_t
dclk_read_raw_time(void)
{
	return slave_time;
}

/**
 * Generate Gaussian random values (taken from
 * http://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform)
 */
double
generate_gaussian_noise(double variance)
{
	static bool haveSpare = false;
	static double rand1, rand2;
 
	if(haveSpare)
	{
		haveSpare = false;
		return sqrt(variance * rand1) * sin(rand2);
	}
 
	haveSpare = true;
 
	rand1 = rand() / ((double) RAND_MAX);
	if(rand1 < 1e-100) rand1 = 1e-100;
	rand1 = -2 * log(rand1);
	rand2 = (rand() / ((double) RAND_MAX)) * TWO_PI;
 
	return sqrt(variance * rand1) * cos(rand2);
}

double next_master_tick = 0.0;
double next_slave_tick = 0.0;

double sim_time = 0.0;

int
main(int argc, char *argv[])
{
	dclk_initialise_state(&dclk);
	bool first_update = true;
	
	printf("#sim_time\tmaster\tslave\terror\tfreq_corr\tphase_corr\n");
	
	while (sim_time < SIM_DURATION) {
		sim_time = MIN(next_master_tick, next_slave_tick);
		
		// Apply corrections
		if (sim_time == next_master_tick && master_time%POLL_PERIOD == 0) {
			dclk_offset_t correction = master_time - dclk_get_time(&dclk);
			correction += generate_gaussian_noise(JITTER_SD*JITTER_SD);
			if (first_update)
				dclk_correct_phase_now(&dclk, correction);
			else
				dclk_add_correction(&dclk, correction);
			first_update = false;
		}
		
		// "Tick" the hardware clocks
		if (sim_time == next_master_tick) {
			next_master_tick += MASTER_TICK_PERIOD;
			master_time++;
		}
		if (sim_time == next_slave_tick) {
			next_slave_tick += SLAVE_TICK_PERIOD;
			next_slave_tick += sin(TWO_PI*sim_time*SLAVE_WANDER_PERIOD)*SLAVE_WANDER_MAGNITUDE;
			slave_time++;
		}
		
		// (Randomly) sample the model state to standard out
		if (((double)rand())/((double)RAND_MAX) < SAMPLE_PROB) {
			dclk_time_t corrected_slave_time = dclk_get_time(&dclk);
			dclk_offset_t error = master_time - corrected_slave_time;
			printf( "%f\t%d\t%d\t%d\t%f\t%f\n" 
			      , sim_time // s
			      , master_time * (5*16) // ns
			      , corrected_slave_time * (5*16) // ns
			      , error * (5*16) // ns
			      , dclk.correction_freq / ((double)(1<<DCLK_FP_FREQ_FBITS)) // Hz
			      , (dclk.correction_phase_accumulator * (5*16)) / ((double)(1<<DCLK_FP_PHASE_FBITS)) // ns
			      );
		}
	}
	
	return 0;
}
