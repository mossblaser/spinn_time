/**
 * SpiNNaker application to test clock synchronisation on large systems. Slave
 * component.
 */

#include <sark.h>
#include <spin1_api.h>

#include "spinn_time_common.h"

// XXX: Makefile is not very good...
#include "dor.c"

// The position of this chip in the system
uint my_x = -1;
uint my_y = -1;
uint my_p = -1;

// The current drift of the hardware counter/timer.
int drift = 0;

// Packet callback on master
void
on_master_mc_packet(uint key, uint payload)
{
	if (payload & PL_PING_BIT) {
		// Respond to ping with the current time ASAP
		uint time = tc2[TC_COUNT] + drift;
		spin1_send_mc_packet(XYPD_TO_KEY(0,0,1,KEY_TO_D(key)), time, TRUE);
		io_printf(IO_BUF, "Pong @ %d!\n"
		         , tc2[TC_COUNT] + drift
		         );
	} else {
		// Apply correction from master
		drift += PL_TO_CORRECTION(payload);
		io_printf(IO_BUF, "Correction %d received, total drift now %d.\n"
		         , PL_TO_CORRECTION(payload)
		         , drift
		         );
	}
}


void
c_main() {
	uint chip_id = spin1_get_chip_id();
	my_x = (chip_id >> 8) & 0xFF;
	my_y = chip_id & 0xFF;
	my_p = spin1_get_core_id();
	
	io_printf(IO_BUF, "Starting spinn_time_slave at %d %d %d...\n", my_x, my_y, my_p);
	
	if (leadAp)
		setup_routing_tables(my_x, my_y, CORES_PER_CHIP);
	
	spin1_callback_on(MCPL_PACKET_RECEIVED, on_master_mc_packet, 0);
	
	tc2[TC_CONTROL] = TC_CONFIG;
	
	spin1_start(TRUE);
}


