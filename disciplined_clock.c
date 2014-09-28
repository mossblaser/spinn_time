#include "disciplined_clock.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)<(b)) ? (b) : (a))
#endif


void
dclk_initialise_state(dclk_state_t *state)
{
	state->last_update_time = dclk_read_raw_time();
	state->last_corrected_time = dclk_read_raw_time();
	state->offset = 0;
	state->correction_freq = 0;
	state->correction_phase_accumulator = 0;
	state->freq_correction_weight = DCLK_FREQ_CORRECTION_WEIGHT_START;
	state->phase_correction_weight = DCLK_PHASE_CORRECTION_WEIGHT_START;
}


dclk_time_t
dclk_get_time(dclk_state_t *state)
{
	dclk_time_t raw_time = dclk_read_raw_time();
	
	// Work out the correction for frequency since the last frequency correction
	dclk_time_t delta_raw_ticks = raw_time - state->last_update_time;
	dclk_offset_t freq_correction = ( ((dclk_offset_t)delta_raw_ticks)
	                                * state->correction_freq
	                                ) >> DCLK_FP_FREQ_FBITS;
	
	// Apply any accumulated phase error while ensuring time monotonicity.
	if (state->correction_phase_accumulator > 0) {
		// Positive, integral phase errors can be incorporated immediately without
		// any impact on monotonicity.
		dclk_offset_t phase_correction = state->correction_phase_accumulator
		                                 >> DCLK_FP_PHASE_FBITS;
		
		state->correction_phase_accumulator -= phase_correction << DCLK_FP_PHASE_FBITS;
		state->offset += phase_correction;
	} else if (state->correction_phase_accumulator < 0) {
		// Negative, integral phase errors can be incorporated only at the rate at
		// which corrected time elapses.
		dclk_time_t new_corrected_time = raw_time + state->offset + freq_correction;
		dclk_time_t corrected_time_since_last_read = new_corrected_time - state->last_corrected_time;
		
		dclk_offset_t phase_correction = -((state->correction_phase_accumulator >> DCLK_FP_PHASE_FBITS) + 1);
		phase_correction = MIN(corrected_time_since_last_read, phase_correction);
		
		state->correction_phase_accumulator += phase_correction << DCLK_FP_PHASE_FBITS;
		state->offset -= phase_correction;
	}
	
	state->last_corrected_time = raw_time + state->offset + freq_correction;
	return state->last_corrected_time;
}


void
dclk_correct_phase_now(dclk_state_t *state, dclk_offset_t correction)
{
	// Apply the desired phase correction immediately
	state->offset += correction;
	
	// Reset the phase accumulator
	state->correction_phase_accumulator = 0;
	
	// Attempt to read the clock which will set state->last_corrected_time but
	// since correction_phase_accumulator is zero, will only account for the
	// current frequency correction meaning future calls will be monotonic.
	dclk_get_time(state);
	
	// Mark now as the last update time such that the frequency estimate in the
	// next true update is usable.
	state->last_update_time = dclk_read_raw_time();
}


void
dclk_add_correction(dclk_state_t *state, dclk_offset_t correction)
{
	dclk_time_t raw_time = dclk_read_raw_time();
	
	dclk_time_t time_since_last_poll = raw_time - state->last_update_time;
	state->last_update_time = raw_time;
	
	if (time_since_last_poll == 0)
		return;
	
	// Update the offset to incorporate the frequency corrections since the last
	// correction was added since the frequency correction may change after this
	// update.
	dclk_offset_t freq_correction = ( ((dclk_offset_t)time_since_last_poll)
	                                * state->correction_freq
	                                ) >> DCLK_FP_FREQ_FBITS;
	state->offset += freq_correction;
	
	// Making the assumptions that neither oscillator has shifted and there is no
	// jitter in the correction measurement, what is the frequency of single-count
	// errors since the last poll? Note that since only one fixed point number is
	// involved, no shifting is required.
	state->correction_freq += ( ((dclk_fp_freq_t)correction * state->freq_correction_weight)
	                          / ((dclk_fp_freq_t)time_since_last_poll)
	                          );
	
	// Accumulate phase corrections of a fraction of the correction. By only
	// applying the corrections fractionally the effect of jitter is reduced at
	// the expense of a longer time constant before the time is corrected. Note
	// converted into fixed point by multiplication with a fixed point number.
	state->correction_phase_accumulator += correction * state->phase_correction_weight;
	
	// Move frequency/phase correction weights down from their starting values
	// towards their target values each time a correction is added. This allows
	// early corrections to be applied more harshly ensuring a quick initial lock.
	if (state->freq_correction_weight > DCLK_FREQ_CORRECTION_WEIGHT_STEP)
		state->freq_correction_weight -= DCLK_FREQ_CORRECTION_WEIGHT_STEP;
	state->freq_correction_weight = MAX( state->freq_correction_weight
	                                   , DCLK_FREQ_CORRECTION_WEIGHT_TARGET
	                                   );
	
	if (state->phase_correction_weight > DCLK_PHASE_CORRECTION_WEIGHT_STEP)
		state->phase_correction_weight -= DCLK_PHASE_CORRECTION_WEIGHT_STEP;
	state->phase_correction_weight = MAX( state->phase_correction_weight
	                                    , DCLK_PHASE_CORRECTION_WEIGHT_TARGET
	                                    );
}