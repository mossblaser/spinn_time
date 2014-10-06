#!/usr/bin/env python

"""
Convert memory dumps from each chip in the system into a CSV of results.
"""

import sys
import struct

WIDTH  = int(sys.argv[1])
HEIGHT = int(sys.argv[2])

print("x,y,num,correction")

for x in range(WIDTH):
	for y in range(HEIGHT):
		num = 0
		with open("corrections/correction_log_%d_%d.dat"%(x,y),"rb") as f:
			# Look for a sentinel value in the first word
			if f.read(4) != b"\xDE\xAD\xBE\xEF"[::-1]:
				break
			
			# Always ignore the first value since it is just setting the clock
			# initially
			f.read(4)
			while True:
				dat = f.read(4)
				if not dat:
					break
				print("%d,%d,%d,%d"%(x,y,num,struct.unpack("<i", dat)[0]))
				num += 1
