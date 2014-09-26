#!/usr/bin/env python

"""
A naive attempt to build an integer-only software clock sync'r.
"""

from math import sin, pi
from random import gauss
from collections import defaultdict

class Master(object):
	
	def __init__( self, event_queue
	            , clock_period = (1.0/200000000.0)*16.0
	            , initial_time = 0
	            , integer_bits = 32
	            ):
		self.event_queue = event_queue
		
		# Time between calls to tick
		self.clock_period = clock_period
		
		# Current time (ticks)
		self.time = initial_time
		
		# Number of bits to use to represent the time
		self.integer_bits = integer_bits
		
		# Schedule initial timer tick
		self.event_queue[0.0].append(self.tick)
	
	
	def tick(self, sim_time):
		self.time += 1
		self.time &= (1<<self.integer_bits)-1
		
		# Reschedule tick
		self.event_queue[sim_time + self.clock_period].append(self.tick)


class Slave(object):
	
	def __init__( self, event_queue, master
	              # Model parameters
	            , clock_period = (1.0/200000000.0)*16.0*(1.0-(30.0/1000000.0))
	            , initial_time = 0
	            , wander_magnitude = (1.0/200000000.0)*16.0*(30.0/1000000.0)
	            , wander_period = 7.0 * 60.0
	            , jitter_sd = 6.0
	            , integer_bits = 32
	            , correction_freq_fbits = 24
	            , correction_phase_fbits = 24
	              # Controller parameters
	            , poll_period = int(5.76 / ((1.0/200000000.0)*16.0))//48
	            , correction_freq_a = 0.1
	            , correction_phase_a = 0.1
	            ):
		self.event_queue = event_queue
		
		# The master clock against which to sync
		self.master = master
		
		# "True" time between calls to tick, not including wander
		self.clock_period = clock_period
		
		# Current raw time (ticks)
		self.raw_time = initial_time
		
		# Offset from raw_time which gives "actual time"
		self.offset = 0
		
		# Sinusoidal wander introduced into clock_period.
		self.wander_magnitude = wander_magnitude
		self.wander_period    = wander_period
		
		# Standard-deviation of jitter around master clock readings
		self.jitter_sd = jitter_sd
		
		# Number of bits in integers used
		self.integer_bits = integer_bits
		
		# Period between polls of the master measured in raw ticks and a counter to
		# measure this.
		self.poll_period = poll_period
		self.time_since_last_poll = 0
		
		# Magnitude indicates adjustment frequency and sign indicates the adjustment
		# value to compensate for the clock's different frequency. Stored as a
		# fixed-point value with self.correction_freq_fbits fractional bits.
		self.correction_freq = 0
		self.correction_freq_fbits = correction_freq_fbits
		
		# The factor by which the exponential frequency average is updated with new
		# correction_freq values. Converted to fixed point.
		self.correction_freq_a = int(round(correction_freq_a * (1<<self.correction_freq_fbits)))
		
		# The fraction of the measured phase error to add to the correction upon an
		# update. Accumulate fractional errors in phase but only apply corrections
		# of integral amounts. (Fixed point)
		self.correction_phase_fbits = correction_phase_fbits
		self.correction_phase_a = int(round(correction_phase_a * (1<<self.correction_phase_fbits)))
		self.fractional_phase_accumulator = 0
		
		# Every self.correction_period raw ticks, add self.correction to the offset.
		self.correction_period = 0
		self.correction = 0
		self.time_since_last_correction = 0
		
		# Schedule initial timer tick
		self.event_queue[0.0].append(self.tick)
	
	
	def clamp(self, value):
		"""
		Clamp a given unsigned integer to self.integer_bits.
		"""
		return value & ((1<<self.integer_bits)-1)
	
	
	def clamps(self, value):
		"""
		Clamp a given signed integer to self.integer_bits.
		"""
		clamped = value & ((1<<self.integer_bits)-1)
		if value < 0:
			clamped |= (-1)<<self.integer_bits
		return clamped
	
	
	@property
	def time(self):
		"""
		Current best guess at the true (master) time.
		"""
		return self.raw_time + self.offset
	
	
	def tick(self, sim_time):
		self.raw_time += 1
		self.raw_time = self.clamp(self.raw_time)
		
		
		# Update time by polling the master at a regular interval
		self.time_since_last_poll += 1
		if self.time_since_last_poll >= self.poll_period:
			# Get a (noisy) error measurement from master
			error = self.master.time - self.time
			error += int(round(gauss(0.0, self.jitter_sd)))
			error = self.clamps(error)
			
			# Making the assumptions that neither oscillator has shifted and there is
			# no jitter in the error measurement, what is the frequency of
			# single-count errors since the last poll? (Fixed point: note that
			# time_since_last_poll is not shifted up in order to save shifting the
			# result up after the division)
			freq = self.clamps(  self.clamps(error * self.correction_freq_a)
			                  // self.clamps(self.time_since_last_poll)
			                  )
			
			# Update the current frequency of corrections
			self.correction_freq += freq
			self.correction_freq = self.clamps(self.correction_freq)
			
			# Set (integer) period of corrections
			if self.correction_freq != 0:
				self.correction_period = self.clamps(  (1<<self.correction_freq_fbits)
				                                    // self.clamps(abs(self.correction_freq))
				                                    )
				self.correction = 1 if self.correction_freq > 0 else -1
			else:
				self.correction = 0
			
			# Correct for the phase difference. Accumulate corrections in fixed point
			# only applying corrections when integral ammounts exist. (Note that
			# the error is converted to fixed point in that a multiplication with an
			# already-fixed point number is performed and then not shifted down).
			self.fractional_phase_accumulator += self.clamps(error * self.correction_phase_a)
			integer_accumulator_value = self.fractional_phase_accumulator >> self.correction_phase_fbits
			if self.fractional_phase_accumulator < 0:
				integer_accumulator_value += 1
			integer_accumulator_value = self.clamps(integer_accumulator_value)
			self.fractional_phase_accumulator -= self.clamps(integer_accumulator_value<<self.correction_phase_fbits)
			self.fractional_phase_accumulator = self.clamps(self.fractional_phase_accumulator)
			self.offset += integer_accumulator_value
			self.offset = self.clamps(self.offset)
			
			
			# Reset the poll timer
			self.time_since_last_poll = 0
		
		
		# Apply periodic corrections to compensate for incorrect frequency
		self.time_since_last_correction += 1
		if self.time_since_last_correction >= self.correction_period:
			self.offset += self.correction
			self.offset = self.clamps(self.offset)
			self.time_since_last_correction = 0
		
		# Reschedule tick (including clock wander)
		clock_period = self.clock_period \
		             + ( sin((sim_time/self.wander_period) * 2.0 * pi)
		               * self.wander_magnitude
		               )
		self.event_queue[sim_time + clock_period].append(self.tick)


if __name__=="__main__":
	from random import random
	
	# Create the model
	event_queue = defaultdict(list)
	master = Master(event_queue)
	slave  = Slave(event_queue, master)
	sim_time = 0
	
	print("Master period:", master.clock_period)
	print("Slave period:", slave.clock_period)
	
	# Result logging
	r_time = []
	r_error = []
	r_correction_freq = []
	r_fractional_phase_acc = []
	
	# Run the simulator
	try:
		while event_queue:
			# Step the simulation
			sim_time = min(event_queue)
			for f in event_queue.pop(sim_time):
				f(sim_time)
			
			if random() < 0.00001:
				print(sim_time, master.time, slave.time, master.time-slave.time)
				r_time.append(sim_time)
				r_error.append((master.time - slave.time) * 80)
				r_correction_freq.append(slave.correction_freq / (1<<slave.correction_freq_fbits))
				r_fractional_phase_acc.append(slave.fractional_phase_accumulator / (1<<slave.correction_phase_fbits))
	except KeyboardInterrupt:
		print("Terminating")
	
	from pylab import *
	
	subplot(3,1,1)
	plot(r_time, r_error)
	xlabel("Time (s)")
	ylabel("Clock Error (ns)")
	
	subplot(3,1,2)
	plot(r_time, r_correction_freq)
	xlabel("Time (s)")
	ylabel("Correction Frequency (Hz)")
	
	subplot(3,1,3)
	plot(r_time, r_fractional_phase_acc)
	xlabel("Time (s)")
	ylabel("Fractional Phase Correction Accumulator")
	
	show()
