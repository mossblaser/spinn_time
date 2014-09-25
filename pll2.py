#!/usr/bin/env python

"""
A naive attempt to build a software clock sync'r.
"""

import random

from math import sin, pi

from collections import defaultdict

# Simulation length in simulator ticks
sim_duration = 100000.0

# A dict {sim_time: [f,...],...} mapping simulation times to functions to call
# at that time.
event_queue = defaultdict(list)

# Values representing the hardware counters of the master and slave respectively
master_clock = 0
slave_clock = 100

# Periods of the master and slave hardware clocks
master_period = 1.0
slave_period = 0.90

# Magnitude of wander on the slave clock
wander_magnitude = 0.0003
wander_period = 10000.0

# Number of (raw) slave clock ticks between attempts to correct the clock
slave_update_period = 500

# A count-down until the next slave update
slave_update_countdown = slave_update_period

# This value is the offset added to the slave_clock by the slave to estimate
# time.
offset = 0

# In a real system, this would be the value correction supplied by the master
# indicating the difference between the slave's estimate of time and the
# master's time.
correction = 0

# The frequency relative to (raw) slave ticks between a correction being applied. If
# 0, never apply a correction. If +ve, add a tick at this interval, if -ve
# subtract one.
avg_correction_freq = 0

# The phase in (raw) slave ticks being adjusted as a kick
last_phase_kick = 0

# A countup to 1/avg_correction_freq - 1
correction_countdown = 0

# The rate at which frequency corrections get applied
avg_correction_freq_tc = 1.0
avg_correction_freq_tc_target = 0.1

# The rate at which phase corrections get applied
avg_correction_phase_tc = 1.0
avg_correction_phase_tc_target = 0.1

# Link jitter (the standard-deviation of normally distributed noise added to
# each error measurement)
jitter_sd = 0.5

def update_master(sim_time):
	global event_queue, master_clock
	
	master_clock += 1
	event_queue[sim_time + master_period].append(update_master)
event_queue[0.0].append(update_master)

def update_slave(sim_time):
	global event_queue, slave_clock, correction, offset, slave_update_countdown
	global avg_correction_freq, correction_countdown, last_phase_kick
	global avg_correction_freq_tc, avg_correction_phase_tc
	
	slave_clock += 1
	event_queue[sim_time + slave_period + sin((sim_time/wander_period) * 2*pi)*wander_magnitude].append(update_slave)
	
	# Apply corrections at the specified frequency
	if avg_correction_freq != 0:
		correction_countdown += 1
		if correction_countdown >= abs(1.0/avg_correction_freq):
			correction_countdown = 0
			correction = int(avg_correction_freq/abs(avg_correction_freq))
		else:
			correction = 0
	else:
		correction = 0
	
	# Work out how much to correct the clock.
	slave_update_countdown -= 1
	if slave_update_countdown == 0:
		slave_update_countdown = slave_update_period
		phase_error = master_clock - (slave_clock + offset)
		phase_error += random.gauss(0.0, jitter_sd)
		freq_error = float(phase_error)/float(slave_update_period)
		
		correction += avg_correction_phase_tc * phase_error
		last_phase_kick = avg_correction_phase_tc * phase_error
		avg_correction_freq += avg_correction_freq_tc * freq_error
		
		# Gradually ramp down the tc
		if avg_correction_freq_tc > avg_correction_freq_tc_target:
			avg_correction_freq_tc *= 0.9
		if avg_correction_phase_tc > avg_correction_phase_tc_target:
			avg_correction_phase_tc *= 0.9
	
	offset += correction
event_queue[0.0].append(update_slave)

# Results
r_time = []
r_master_time = []
r_slave_time = []
r_error = []
r_avg_correction_freq = []
r_correction = []
r_phase_kicks = []

# Run the simulator
sim_time = 0.0
while event_queue and sim_time < sim_duration:
	# Step the simulation
	sim_time = min(event_queue)
	for f in event_queue.pop(sim_time):
		f(sim_time)
	
	# Record results
	r_time.append(sim_time)
	r_master_time.append(master_clock)
	r_slave_time.append(slave_clock + offset)
	r_error.append(master_clock - (slave_clock + offset))
	r_avg_correction_freq.append(avg_correction_freq)
	r_correction.append(correction)
	r_phase_kicks.append(last_phase_kick)


from pylab import *

figure(0)
subplot(3,1,1)
plot(r_time, r_error)
xlabel("(Real) Time")
ylabel("Clock Error")

subplot(3,1,2)
plot(r_time, r_avg_correction_freq)
xlabel("(Real) Time")
ylabel("Average Correction Frequency")

subplot(3,1,3)
plot(r_time, r_phase_kicks)
xlabel("(Real) Time")
ylabel("Most recent phase kick")

show()
