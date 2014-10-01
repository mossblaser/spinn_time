/**
 * SpiNNaker application to test clock synchronisation on large systems.
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
uint slave = 0;

////////////////////////////////////////////////////////////////////////////////
// Slave-Specific Code
////////////////////////////////////////////////////////////////////////////////

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
	return TIMER_VALUE;
}

// Flash the LEDs at a regular interval synchronised by the timer
void
on_slave_tick(uint _1, uint _2)
{
	// Work out when the next toggle will be
	dclk_time_t now = dclk_get_time(&dclk);
	now += 100;
	dclk_time_t num_toggles = now / LED_TOGGLE_PERIOD;
	dclk_time_t next_toggle = LED_TOGGLE_PERIOD * (num_toggles+1);
	
	// Schedule the next timer interrupt (making sure its at least one cycle in
	// the future
	dclk_offset_t next_toggle_raw_ticks = dclk_get_ticks_until_time(&dclk, next_toggle);
	tc1[TC_LOAD] = MAX(next_toggle_raw_ticks, 1);
	
	// Set the LED state
	spin1_led_control((num_toggles%2) ? LED_ON(0) : LED_OFF(0));
	io_printf(IO_BUF, "LED %s\n", (num_toggles%2)?"on":"off");
}


// Discipline the clock based on corrections from the master
void
on_slave_mc_packet(uint key, uint payload)
{
	if (payload & PL_PING_BIT) {
		// Respond to ping with the current time ASAP
		uint time = dclk_get_time(&dclk);
		spin1_send_mc_packet(RETURN_KEY(key), time, TRUE);
	} else {
		// Apply correction from master
		if (result_count) {
			// Sanity check
			if (PL_TO_CORRECTION(payload) < 1000 && PL_TO_CORRECTION(payload) > -1000)
				dclk_add_correction(&dclk, PL_TO_CORRECTION(payload));
			else
				io_printf(IO_BUF, "The following correction was ignored:\n.");
			
			// Start the interrupts!
			if (result_count == 1) {
				tc1[TC_LOAD] = 0;
				tc1[TC_CONTROL] = ( (1 << 0) /* One-shot counter */ \
				                  | (1 << 1) /* 32-bit counter */ \
				                  | (1 << 2) /* Clock divider (/1 = 0, /16 = 1, /256 = 2) */ \
				                  | (1 << 5) /* Interrupt */ \
				                  | (1 << 6) /* Periodic */ \
				                  | (1 << 7) /* Enabled */ \
				                  );
				// Stop the monitor flashing the LEDs
				sv->led_period = 0;
			}
		} else {
			dclk_correct_phase_now(&dclk, PL_TO_CORRECTION(payload));
		}
		
		io_printf(IO_BUF, "Correction %d received via DOR %d. Corr Freq: 0x%08x\n"
		         , PL_TO_CORRECTION(payload)
		         , KEY_TO_D(key)
		         , dclk.correction_freq
		         );
		if (NUM_CORRECTIONS != 0)
			*(result_log++) = PL_TO_CORRECTION(payload);
		
		// Terminate after enough updates have ocurred
		if (++result_count > NUM_CORRECTIONS && (NUM_CORRECTIONS != 0))
			spin1_exit(0);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Master-Specific Code
////////////////////////////////////////////////////////////////////////////////

// Timer for master sending out requests (us)
#define MASTER_TIMER_TICK 1000

// A lookup table of dimension orders known to work with each remote core
unsigned char working_dimension_order [WIDTH][HEIGHT][CORES_PER_CHIP];

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
on_master_mc_packet(uint return_key, uint remote_time)
{
	// Reject packets with the wrong key
	if (RETURN_KEY(key) != return_key)
		return;
	
	// Calculate the approximate error in the remote clock
	uint recv_time = TIMER_VALUE;
	uint latency = (recv_time - send_time)/2;
	remote_time += latency;
	last_error = (((int)TIMER_VALUE) - ((int)remote_time));
	got_ping = TRUE;
	
	// Send a correction back
	spin1_send_mc_packet(key, (~PL_PING_BIT) & last_error, TRUE);
}

// Send out pings and corrections to each slave
void
on_master_tick(uint _1, uint _2)
{
	static uint num_responses = 0;
	static uint num_missing = 0;
	static uint num_scans = 0;
	static int total_drift = 0;
	
	// Try a different DOR if a ping doesn't make it
	if (!got_ping) {
		working_dimension_order[dest_x][dest_y][dest_p-1] ++;
		working_dimension_order[dest_x][dest_y][dest_p-1] %= NUM_DIM_ORDERS;
		num_missing++;
	} else if ((last_error > 1000 || last_error < -1000) && num_scans > 6) {
		io_printf(IO_BUF, "%d,%d,%d has very large error %d.\n"
		         , dest_x, dest_y, dest_p-1
		         , last_error
		         );
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
					num_scans++;
				}
			}
		}
	} while (dest_x == 0 && dest_y == 0 && dest_p == 1);
	
	// Send an empty packet to the remote to ping back
	key = XYPD_TO_KEY(dest_x,dest_y,dest_p-1, working_dimension_order[dest_x][dest_y][dest_p-1]);
	got_ping = FALSE;
	spin1_send_mc_packet(key, PL_PING_BIT, TRUE);
	send_time = TIMER_VALUE;
}


////////////////////////////////////////////////////////////////////////////////
// Shared code
////////////////////////////////////////////////////////////////////////////////


void
c_main() {
	uint chip_id = spin1_get_chip_id();
	my_x = (chip_id >> 8) & 0xFF;
	my_y = chip_id & 0xFF;
	my_p = spin1_get_core_id();
	slave = !((my_x==0) && (my_y==0) && (my_p==1));
	
	dclk_initialise_state(&dclk);
	
	io_printf( IO_BUF, "Starting spinn_time at %d %d %d as %s...\n"
	         , my_x, my_y, my_p
	         , slave ? "slave" : "master"
	         );
	
	if (leadAp)
		setup_routing_tables(my_x, my_y, CORES_PER_CHIP);
	
	
	tc2[TC_CONTROL] = TC_CONFIG;
	
	if (slave) {
		spin1_callback_on(TIMER_TICK, on_slave_tick, 1);
		spin1_callback_on(MCPL_PACKET_RECEIVED, on_slave_mc_packet, 0);
		
		result_log = (int *)(SDRAM_BASE_BUF);
		// Add sentinel at start and zero the result array
		*(result_log++) = 0xDEADBEEF;
		for (int i = 0; i < NUM_CORRECTIONS; i++)
			result_log[i] = 0;
	} else {
		spin1_set_timer_tick(MASTER_TIMER_TICK);
		spin1_callback_on(TIMER_TICK, on_master_tick, 0);
		spin1_callback_on(MCPL_PACKET_RECEIVED, on_master_mc_packet, 1);
		
		tc2[TC_CONTROL] = TC_CONFIG;
		
		// Initialise DOR lookup
		for (int x = 0; x < WIDTH; x++)
			for (int y = 0; y < HEIGHT; y++)
				for (int p = 0; y < CORES_PER_CHIP; y++)
					working_dimension_order[x][y][p] = DIM_ORDER_XYZ;
		
		// Remove any sentinel in SDRAM left by running the slave...
		*((uint*)SDRAM_BASE_BUF) = 0;
	}
	
	spin1_start(TRUE);
}


