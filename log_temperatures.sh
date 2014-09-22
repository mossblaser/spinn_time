# Output CSV headers
echo "time,rack,slot,tempn,temps,fan0,fan1"

# Poll the temperatures
START="$(date +%s)"
while sleep 5; do
	NOW="$(date +%s)"
	TIME=$(($NOW-$START))
	for IP in 10.2.{225..229}.0; do
		for SLOT in {0..23}; do
			echo "sp $SLOT";
			echo "temp";
			echo "sver";
		done | bmpc $IP;
	done | awk "BEGIN {time=$TIME} "'
		/.*temp$/ {tn="NA";ts="NA";fan0="NA";fan1="NA"};
		/T_intN/ {tn=$2;}
		/T_intS/ {ts=$2;}
		/Fan0/ {fan0=$2;}
		/Fan1/ {fan1=$2;}
		/sver/ {
			split($1, IP_PARTS, ":");
			split(IP_PARTS[1], IP, ".");
			rack=IP[3]-225;
			slot=IP_PARTS[2];
			printf "%d,%d,%d,%s,%s,%s,%s\n", time,rack,slot,tn,ts,fan0,fan1;
		}
	'
done
