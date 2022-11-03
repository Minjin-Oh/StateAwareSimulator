mkdir ./scale_res

for var in $(seq 8)
do
	cp ./scaletest$var/test03-single-1/lifetime.csv ./scale_res/lifetime-$var.csv
done
