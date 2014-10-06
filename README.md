SpiNN-Time: Clock synchronisation in SpiNNaker
==============================================

SpiNNaker is a super-computer designed for the large-scale simulation of spiking
neural networks and consists of a custom network of up to one million ARM CPUs
each of which simulating the behaviour of hundreds or thousands of neurons. The
state of each neuron is (typically) recomputed every millisecond and any
resulting spikes are multicast throughout the network to other connected
neurons. This millisecond update is controlled by a simple timer-counter in each
CPU core. Since these timers are all independent, they may be initially
out-of-phase and, if the underlying oscillator is not shared, their frequencies
may cause them to drift appart over time. Because of this, simulations cannot be
deterministic and the models behaviour is nolonger consistent (since the model
assumes a globally synchronised state update computation.

This repository contains a number of experiments attempting to solve this
problem by synchronising the clocks throughout a SpiNNaker system. Also included
is an initial implementation of a synchronisation algorithm and a suitable
testbench for SpiNNaker along with a standalone software testbench.
