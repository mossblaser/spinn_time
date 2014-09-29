require("ggplot2")
require("gridExtra")

t <- read.csv("/tmp/tmps")

t_by_rack <- aggregate(.~rack+time, t, mean)

base_plot <- ggplot(t_by_rack, aes(x=time,group=rack,color=factor(rack))) +
	labs(x="Time (s)") +
	labs(color="Rack") +
	labs(linetype="Sensor")

temp_plot <- base_plot +
	geom_line(aes(y=tempn,linetype="Top")) +
	geom_line(aes(y=temps,linetype="Bottom")) +
	labs(y="Mean Temperature (C)")

fan_plot <- base_plot +
	geom_line(aes(y=fan0,linetype="Fan 0")) +
	geom_line(aes(y=fan1,linetype="Fan 1")) +
	labs(y="Mean Fan Speed (RPM)")

grid.arrange(temp_plot, fan_plot)
