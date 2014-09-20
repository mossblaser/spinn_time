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
#define MASTER_TIMER_TICK 500

// Address in SDRAM to store results
#define RESULT_ADDR(x,y,p,d) ( ((uint*)SDRAM_BASE_BUF) \
                               + (( (((x) + ((y)*WIDTH)) * CORES_PER_CHIP) \
                                  + (p)-1) * NUM_DIM_ORDERS) \
                               + (d) \
                             )

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
volatile uint got_ping;

// Packet callback on master
void
on_master_mc_packet(uint _, uint remote_time)
{
	// Calculate the approximate error in the remote clock
	uint recv_time = tc2[TC_COUNT];
	uint latency = (recv_time - send_time)/2;
	remote_time += latency;
	last_error = (int)(tc2[TC_COUNT] - remote_time);
	got_ping = TRUE;
	
	// Send a correction back
	spin1_send_mc_packet(key, (~PL_PING_BIT) & last_error, TRUE);
}

// Send out pings and corrections to each slave
void
on_tick(uint _1, uint _2)
{
	// Store results
	if (got_ping)
		*(RESULT_ADDR(dest_x,dest_y,dest_p,dest_dim_order)) = last_error;
	else
		*(RESULT_ADDR(dest_x,dest_y,dest_p,dest_dim_order)) = 0xDEADBEEF;
	
	// Advance through the system
	do {
		if (++dest_x >= WIDTH) {
			dest_x = 0;
			if (++dest_y >= HEIGHT) {
				dest_y = 0;
				if (++dest_p > CORES_PER_CHIP) {
					dest_p = 1;
					if (++dest_dim_order >= NUM_DIM_ORDERS) {
						dest_dim_order = 0;
					}
				}
				io_printf(IO_BUF, "Next core/dim-order...\n");
			}
		}
	} while (dest_x == 0 && dest_y == 0 && dest_p == 1);
	
	// Send an empty packet to the remote to ping back
	key = XYPD_TO_KEY(dest_x,dest_y,dest_p, dest_dim_order);
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
	
	spin1_start(TRUE);
}


