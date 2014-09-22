/**
 * SpiNNaker application to test clock synchronisation on large systems. Master
 * component.
 */

#include <sark.h>
#include <spin1_api.h>

#include "spinn_time_common.h"

// XXX: Makefile is not very good...
#include "dor.c"

// Timer for master sending out requests (us)
#define MASTER_TIMER_TICK 1000

// A lookup table of dimension orders known to work with each remote core
unsigned char working_dimension_order [WIDTH][HEIGHT][CORES_PER_CHIP];

// The position of this chip in the system
uint my_x = -1;
uint my_y = -1;
uint my_p = -1;

// Last destination sent to (on master)
uint dest_x = 0;
uint dest_y = 0;
uint dest_p = 1;
uint dest_dim_order = 0;

// Time of last packet transmission
volatile uint send_time;

// The key of the last packet to be sent
volatile uint key;

// Values to record as results
volatile int  last_error;
volatile uint got_ping = TRUE;

// Packet callback on master
void
on_master_mc_packet(uint _, uint remote_time)
{
	// Calculate the approximate error in the remote clock
	uint recv_time = tc2[TC_COUNT];
	uint latency = (recv_time - send_time)/2;
	remote_time += latency;
	last_error = (((int)tc2[TC_COUNT]) - ((int)remote_time));
	got_ping = TRUE;
	
	// Send a correction back
	spin1_send_mc_packet(key, (~PL_PING_BIT) & last_error, TRUE);
}

// Send out pings and corrections to each slave
void
on_tick(uint _1, uint _2)
{
	static uint num_responses = 0;
	static uint num_missing = 0;
	static int total_drift = 0;
	
	// Try a different DOR if a ping doesn't make it
	if (!got_ping) {
		working_dimension_order[dest_x][dest_y][dest_p] ++;
		working_dimension_order[dest_x][dest_y][dest_p] %= NUM_DIM_ORDERS;
		num_missing++;
	} else {
		num_responses++;
		total_drift += (last_error >= 0) ? last_error : -last_error;
	}
	
	// Advance through the system
	do {
		if (++dest_x >= WIDTH) {
			dest_x = 0;
			if (++dest_y >= HEIGHT) {
				dest_y = 0;
				if (++dest_p > CORES_PER_CHIP) {
					dest_p = 1;
					
					io_printf( IO_BUF, "Full scan complete, %d updated, %d not responding, total drift = %d.\n"
					         , num_responses
					         , num_missing
					         , total_drift
					         );
					num_responses = 0;
					num_missing = 0;
					total_drift = 0;
				}
			}
		}
	} while (dest_x == 0 && dest_y == 0 && dest_p == 1);
	
	// Send an empty packet to the remote to ping back
	key = XYPD_TO_KEY(dest_x,dest_y,dest_p, working_dimension_order[dest_x][dest_y][dest_p]);
	got_ping = FALSE;
	spin1_send_mc_packet(key, PL_PING_BIT, TRUE);
	send_time = tc2[TC_COUNT];
}


void
c_main() {
	uint chip_id = spin1_get_chip_id();
	my_x = (chip_id >> 8) & 0xFF;
	my_y = chip_id & 0xFF;
	my_p = spin1_get_core_id();
	
	io_printf(IO_BUF, "Starting spinn_time_master at %d %d %d...\n", my_x, my_y, my_p);
	
	if (leadAp)
		setup_routing_tables(my_x, my_y, CORES_PER_CHIP);
	
	spin1_set_timer_tick(MASTER_TIMER_TICK);
	spin1_callback_on(TIMER_TICK, on_tick, 0);
	spin1_callback_on(MCPL_PACKET_RECEIVED, on_master_mc_packet, 1);
	
	tc2[TC_CONTROL] = TC_CONFIG;
	
	// Initialise DOR lookup
	for (int x = 0; x < WIDTH; x++)
		for (int y = 0; y < HEIGHT; y++)
			for (int p = 0; y < CORES_PER_CHIP; y++)
				working_dimension_order[x][y][p] = DIM_ORDER_XYZ;
	
	// Remove any sentinel in SDRAM left by running the slave...
	*((uint*)SDRAM_BASE_BUF) = 0;
	
	spin1_start(TRUE);
}


