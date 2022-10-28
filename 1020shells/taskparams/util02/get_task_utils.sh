for var in $(seq 50)
do
	sed -n '5p' taskparam-$var.csv > result-$var.csv
#	sed -i "s/$/\n/g" result-$var.csv
done
cat result-*.csv > agg.csv


