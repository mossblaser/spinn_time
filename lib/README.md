C Libraries
===========

This directory contains three C libraries for clock synchronisation experiments.

* `dor.{c,h}` is a library which generates simple dimension-order-routing tables
  for SpiNNaker which are used in these experiments.

* `disciplined_clock.{c,h}` is a library which implements the clock
  synchronisation algorithm.

* `disciplined_timer.{c,h}` is a support library for SpiNNaker which will
  control Timer 1 to remain in sync with a clock synced using the
  disciplined clock library.
