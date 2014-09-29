/**
 * SpiNNaker application to test clock synchronisation on large systems. Slave
 * component.
 */

#include <sark.h>
#include <spin1_api.h>

#include "spinn_time_common.h"

// XXX: Makefile is not very good...
#include "dor.c"
#include "disciplined_clock.c"

// The position of this chip in the system
uint my_x = -1;
uint my_y = -1;
uint my_p = -1;

// The current drift of the hardware counter/timer.
int drift = 0;

// Log the drift over time in SDRAM.
int *result_log;
uint result_count = 0;

// Disciplined clock algorithm state
dclk_state_t dclk;

dclk_time_t
dclk_read_raw_time(void)
{
	return tc2[TC_COUNT];
}


void
on_slave_mc_packet(uint key, uint payload)
{
	if (payload & PL_PING_BIT) {
		// Respond to ping with the current time ASAP
		uint time = dclk_get_time(&dclk);
		spin1_send_mc_packet(RETURN_KEY(key), time, TRUE);
	} else {
		// Apply correction from master
		if (result_count)
			dclk_add_correction(&dclk, PL_TO_CORRECTION(payload));
		else
			dclk_correct_phase_now(&dclk, PL_TO_CORRECTION(payload));
		
		io_printf(IO_BUF, "Correction %d received via DOR %d. Corr Freq: 0x%08x\n"
		         , PL_TO_CORRECTION(payload)
		         , KEY_TO_D(key)
		         , dclk.correction_freq
		         );
		*(result_log++) = PL_TO_CORRECTION(payload);
		
		// Terminate after enough updates have ocurred
		if (++result_count > NUM_CORRECTIONS)
			spin1_exit(0);
	}
}


void
c_main() {
	uint chip_id = spin1_get_chip_id();
	my_x = (chip_id >> 8) & 0xFF;
	my_y = chip_id & 0xFF;
	my_p = spin1_get_core_id();
	
	dclk_initialise_state(&dclk);
	
	io_printf(IO_BUF, "Starting spinn_time_slave at %d %d %d...\n", my_x, my_y, my_p);
	
	if (leadAp)
		setup_routing_tables(my_x, my_y, CORES_PER_CHIP);
	
	spin1_callback_on(MCPL_PACKET_RECEIVED, on_slave_mc_packet, 0);
	
	tc2[TC_CONTROL] = TC_CONFIG;
	
	result_log = (int *)(SDRAM_BASE_BUF);
	// Add sentinel at start
	*(result_log++) = 0xDEADBEEF;
	for (int i = 0; i < NUM_CORRECTIONS; i++)
		result_log[i] = 0;
	
	spin1_start(TRUE);
}


