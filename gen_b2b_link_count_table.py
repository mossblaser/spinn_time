#!/usr/bin/env python

"""
A script which takes a (non-toroidal) system size and generates a CSV file
informing the user of how many chip-to-chip and board-to-board hops it would
take to reach a given core setting off from (0,0) using a particular dimension
ordering.

Usage:
	python gen_b2b_link_count_table [sys_width] [sys_height] > output.csv
"""

# Given an x and y chip position modulo 12, return the offset of the board's
# bottom-left chip from the chip's position subtract its modulo. Note that this
# table is rendered upside-down (y is ascending downards). 
# Usage: CENTER_OFFSET[y][x][dimension] where dimension is 0 for x and 1 for y.
CENTER_OFFSET = (
	#X:    1        2        3        4        5        6        7        8        9       10       11       12   # Y:
	((+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+4,-4), (+4,-4), (+4,-4), (+4,-4), (+4,-4), (+4,-4), (+4,-4)), #  1
	((+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+4,-4), (+4,-4), (+4,-4), (+4,-4), (+4,-4), (+4,-4)), #  2
	((+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+4,-4), (+4,-4), (+4,-4), (+4,-4), (+4,-4)), #  3
	((+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+4,-4), (+4,-4), (+4,-4), (+4,-4)), #  4
	((-4,+4), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+8,+4), (+8,+4), (+8,+4), (+8,+4)), #  5
	((-4,+4), (-4,+4), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+8,+4), (+8,+4), (+8,+4), (+8,+4)), #  6
	((-4,+4), (-4,+4), (-4,+4), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+8,+4), (+8,+4), (+8,+4), (+8,+4)), #  7
	((-4,+4), (-4,+4), (-4,+4), (-4,+4), (+0,+0), (+0,+0), (+0,+0), (+0,+0), (+8,+4), (+8,+4), (+8,+4), (+8,+4)), #  8
	((-4,+4), (-4,+4), (-4,+4), (-4,+4), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+8,+4), (+8,+4), (+8,+4)), #  9
	((-4,+4), (-4,+4), (-4,+4), (-4,+4), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+8,+4), (+8,+4)), # 10
	((-4,+4), (-4,+4), (-4,+4), (-4,+4), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+8,+4)), # 11
	((-4,+4), (-4,+4), (-4,+4), (-4,+4), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8), (+4,+8)), # 12
)

# Get the x-y coordinate of the chip at (0,0) on the same board (x,y). Returns a
# vector (x,y).
def get_chip_board(x,y):
	xx = x%12
	yy = y%12
	dx,dy = CENTER_OFFSET[yy][xx]
	return ( (x - xx) + dx
	       , (y - yy) + dy
	       )


# Get the minimal 3D hexagonal coordinate for a given x/y
def get_hex_coord(x,y):
	return (x-min(x,y), y-min(x,y), -min(x,y))

# A lookup for dimension orders. Usage: DIM_ORDER_INDEX[dim_order][index] where
# for a given dimension order, returns the dimension index for the index-th
# dimension.
DIM_ORDER_INDEX = ( (0,1,2) # XYZ (0)
                  , (0,2,1) # XZY (1)
                  , (1,0,2) # YXZ (2)
                  , (1,2,0) # YZX (3)
                  , (2,0,1) # ZXY (4)
                  , (2,1,0) # ZYX (5)
                  )


# Count the number of board-to-board links crossed taking a given
# dimension-order route from (0,0) to the specified target. Dimension orders
# start from 0 (as in the results file).
def count_board_to_board_links(target_x,target_y, dimension_order):
	cur_pos   = [0,0,0]
	delta_xyz = list(get_hex_coord(target_x,target_y))
	b2b = 0
	
	# Dimension indexes
	d = DIM_ORDER_INDEX[dimension_order]
	for di in range(3):
		while delta_xyz[d[di]]:
			# Get the sign of the current dimension
			if delta_xyz[d[di]] > 0:
				s = 1
			else:
				s = -1
			
			old_board = get_chip_board(cur_pos[0]-cur_pos[2], cur_pos[1]-cur_pos[2])
			cur_pos[d[di]] += s
			new_board = get_chip_board(cur_pos[0]-cur_pos[2], cur_pos[1]-cur_pos[2])
			
			delta_xyz[d[di]] -= s
			
			if old_board != new_board:
				b2b += 1
	
	return b2b


if __name__=="__main__":
	import sys
	width  = int(sys.argv[1])
	height = int(sys.argv[2])
	
	print("x,y,d,c2c,b2b")
	for x in range(width):
		for y in range(height):
			for dim_order in range(len(DIM_ORDER_INDEX)):
				b2b = count_board_to_board_links(x,y,dim_order)
				c2c = sum(map(abs, get_hex_coord(x,y))) - b2b
				print("%d,%d,%d,%d,%d"%(
					x,y,dim_order,c2c,b2b
				))
