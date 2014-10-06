/**
 * SpiNNaker application which measures roundtrip to every core in the system
 * from (0,0). Dumps the result in SDRAM on (0,0).
 */

#include <sark.h>
#include <spin1_api.h>

#include "dor.h"

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

// Got reply flag
volatile uint got_reply = FALSE;

// Packet send time
volatile uint send_time;
volatile uint recv_time;

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
	
	io_printf(IO_BUF, "Starting latency_experiment as %d %d %d...\n", my_x, my_y, my_p);
	
	if (leadAp) {
		setup_routing_tables();
		
		// Set router timeout
		volatile uint *control_reg = (uint*)(RTR_BASE+RTR_CONTROL);
		uint control = *control_reg;
		control &= 0xFFFF;
		control |= (WAIT2<<24) | (WAIT1<<16);
		*control_reg = control;
	}
	
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
