library(ggplot2)

################################################################################
# SpiNNaker topology utility functions
################################################################################

# Given an x and y chip position modulo 12, return the offset of the board's
# bottom-left chip from the chip's position subtract its modulo. Note that this
# table is rendered upside-down (y is ascending downards). Also note that this
# is indexed from 1, not 0 as in results files.
# Usage: CENTER_OFFSET[x,y, dimension] where dimension is 1 for x and 2 for y.
CENTER_OFFSET <- aperm(array(c(
	#X: 1      2      3      4      5      6      7      8      9     10     11     12  # Y:
	+0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +4,-4, +4,-4, +4,-4, +4,-4, +4,-4, +4,-4, +4,-4, #  1
	+0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +4,-4, +4,-4, +4,-4, +4,-4, +4,-4, +4,-4, #  2
	+0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +4,-4, +4,-4, +4,-4, +4,-4, +4,-4, #  3
	+0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +4,-4, +4,-4, +4,-4, +4,-4, #  4
	-4,+4, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +8,+4, +8,+4, +8,+4, +8,+4, #  5
	-4,+4, -4,+4, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +8,+4, +8,+4, +8,+4, +8,+4, #  6
	-4,+4, -4,+4, -4,+4, +0,+0, +0,+0, +0,+0, +0,+0, +0,+0, +8,+4, +8,+4, +8,+4, +8,+4, #  7
	-4,+4, -4,+4, -4,+4, -4,+4, +0,+0, +0,+0, +0,+0, +0,+0, +8,+4, +8,+4, +8,+4, +8,+4, #  8
	-4,+4, -4,+4, -4,+4, -4,+4, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8, +8,+4, +8,+4, +8,+4, #  9
	-4,+4, -4,+4, -4,+4, -4,+4, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8, +8,+4, +8,+4, # 10
	-4,+4, -4,+4, -4,+4, -4,+4, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8, +8,+4, # 11
	-4,+4, -4,+4, -4,+4, -4,+4, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8, +4,+8  # 12
), dim = c(2,12,12)), c(2,3,1))

# Get the x-y coordinate of the chip at (0,0) on the same board (x,y). Returns a
# vector (x,y).
get_chip_board <- function(x,y) {
	xx = x %% 12
	yy = y %% 12
	return(c( ((x - xx) + CENTER_OFFSET[xx+1,yy+1,1])
	        , ((y - yy) + CENTER_OFFSET[xx+1,yy+1,2])
	        ))
}


# Get the minimal 3D hexagonal coordinate for a given x/y
get_hex_coord <- function(x,y) {
	return(c(x,y,0) - min(x,y))
}

# A lookup for dimension orders. An array [dim_order, index] where for a given
# dimension order, returns the dimension index for the index-th dimension. Note
# that dimension numbers are indexed from 1 (rather than zero in the results
# files).
DIM_ORDER_INDEX <- aperm(array(c(
	# XYZ (0 in results file)
	1,2,3,
	# XZY (1 in results file)
	1,3,2,
	# YXZ (2 in results file)
	2,1,3,
	# YZX (3 in results file)
	2,3,1,
	# ZXY (4 in results file)
	3,1,2,
	# ZYX (5 in results file)
	3,2,1
), dim = c(3,6)), c(2,1))


# Count the number of board-to-board links crossed taking a given
# dimension-order route from (0,0) to the specified target. Dimension orders
# start from 0 (as in the results file).
count_board_to_board_links <- function( target_x,target_y
                                      , dimension_order
                                      ) {
	cur_pos   <- c(0,0,0)
	delta_xyz <- get_hex_coord(target_x,target_y)
	b2b <- 0
	
	# Dimension indexes
	d = DIM_ORDER_INDEX[dimension_order+1,]
	for (di in c(1:3)) {
		while (delta_xyz[d[di]] != 0) {
			s = sign(delta_xyz[d[di]])
			
			old_board <- get_chip_board(cur_pos[1]-cur_pos[3], cur_pos[2]-cur_pos[3])
			cur_pos[d[di]] <- cur_pos[d[di]] + s
			new_board <- get_chip_board(cur_pos[1]-cur_pos[3], cur_pos[2]-cur_pos[3])
			
			delta_xyz[d[di]] <- delta_xyz[d[di]] - s
			
			if (old_board[1] != new_board[1] || old_board[2] != new_board[2]) {
				b2b <- b2b + 1
			}
		}
	}
	
	return(b2b)
}


################################################################################
# Data dump loader
################################################################################

# Read the script's arguments
read.latencyResults <- function ( filename
                                , width_chips
                                , height_chips
                                , cores_per_chip = 16
                                , num_dim_orders = 6
                                ) {
	# Get the x/y/p/d columns of the results table
	results <- expand.grid( d=c(0:(num_dim_orders-1))
	                      , p=c(0:(cores_per_chip-1))
	                      , x=c(0:(width_chips-1))
	                      , y=c(0:(height_chips-1))
	                      )
	
	# Read the roundtrips from the file
	f = file(filename,"rb")
	results$roundtrip <- readBin( f
	                            , integer()
	                            , n=nrow(results)
	                            , size=4
	                            , endian="little"
	                            )
	close(f)
	
	# Remove null results
	results <- results[results$roundtrip!=0,]
	
	# Remove the "bounceback" results
	results <- results[results$x!=0 | results$y!=0 | results$p!=0,]
	
	# Add distance measure
	results$distance <- unlist(Map( function(x,y) sum(abs(c(x,y,0)-min(x,y)))
	                              , results$x
	                              , results$y
	                              )
	                          )
	
	# Scale to us
	results$roundtrip <- results$roundtrip * 0.005
	
	return(results)
}


# Add to results a field indicating chip to chip and board to board hops
# encountered by each journey. These fields are loaded from a file computed by
# an external python script on account of R's fantastically poor performance.
add_c2c_and_b2b <- function (result_frame, count_filename) {
	return(merge(result_frame, read.csv(count_filename, header=TRUE)))
}



################################################################################
# Convenience data-processing functions
################################################################################

# Plot a heatmap aggregated by chip
roundtrip_heatmap <- function(results, fun = min) {
	ggplot( aggregate(roundtrip ~ x+y, results, fun)
	      , aes(x=x, y=y, fill=roundtrip)
	      ) + geom_tile()
}


# Return the aggregate roundtrip time to a particular chip
chip_roundtrip <- function(results, x,y, fun=min) {
	return(fun(results[results$x == x & results$y == y, ]$roundtrip))
}
