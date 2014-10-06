require("ggplot2")
require("gridExtra")

c <- read.csv("corrections.csv")
c$time <- c$num * (0.001*96*60)

corrections_plot <- ggplot(c, aes(x=time, y=correction, group=(x+(y*96)), color=(x+(y*96)))) +
	geom_line() +
	labs("Clock corrections over time for every chip.") +
	labs(color="Chip ID") +
	labs(x="Time (s)") +
	labs(y="Correction applied (ticks)")


dydx <- function(values) {
	return(head(values, -1) - tail(values, -1));
}


c_sd <- aggregate(correction~x+y, c, sd)
corrections_sd_plot <- ggplot(c_sd, aes(x=x,y=y,fill=correction)) +
	geom_tile() +
	labs("Correction variation for each chip") +
	labs(fill="SD")

c_median <- aggregate(correction~x+y, c, median)
corrections_median_plot <- ggplot(c_median, aes(x=x,y=y,fill=correction)) +
	geom_tile() +
	labs("Median corrections for each chip") +
	labs(fill="Ticks")


