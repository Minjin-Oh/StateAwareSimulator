for var in $(seq 4)
do
	cp ./skewnesstest_w/test02-wskew$var/lifetime.csv ./lifetimes/skew_w/lifetime-ws$var.csv
done
for var in $(seq 4)
do
	cp ./skewnesstest_r/test02-rskew$var/lifetime.csv ./lifetimes/skew_r/lifetime-rs$var.csv
done
