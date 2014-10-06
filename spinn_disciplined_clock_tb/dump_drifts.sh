for X in {0..95}; do
	for Y in {0..59}; do
		echo "sp $X $Y"
		echo "sdump corrections/correction_log_${X}_${Y}.dat 70000000 fa0"
	done
done | ybug 10.2.225.1

python read_spinn_time_results.py 96 60 > corrections.csv
