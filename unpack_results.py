#!/usr/bin/env python

"""
Convert a memory dump from SpiNNaker into a table with columns x,y,p,roundtrip
(ticks).
"""

import sys
import struct

result_file    = sys.argv[1]
width          = int(sys.argv[2])
height         = int(sys.argv[3])
cores_per_chip = int(sys.argv[4])
num_dim_orders = 6

with open(result_file, "rb") as f:
	x = 0
	y = 0
	p = 0
	d = 0
	roundtrips = []
	while True:
		data = f.read(4)
		if not data:
			break
		
		roundtrip = struct.unpack("<L", data)[0]
		if roundtrip != 0:
			roundtrips.append(roundtrip)
		
		d += 1
		if d >= num_dim_orders:
			d = 0
			p += 1
			if p >= cores_per_chip:
				p = 0
				print("%d\t%d\t%d"%(
					x,y,
					( sum(roundtrips)/len(roundtrips)
					  if roundtrips else 0
					)
				))
				roundtrips = []
				x += 1
				if x >= width:
					x = 0
					print("") # Empty line for gnuplot
					y += 1
					if y >= height:
						y = 0
