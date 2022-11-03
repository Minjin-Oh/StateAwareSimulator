mkdir ./skew_test
mkdir ./skew_test/1
mkdir ./skew_test/2
mkdir ./skew_test/3
mkdir ./skew_test/4

for var in $(seq 1 12)
do
	cp ./skew-pt1/test02-1-task/taskset-$var/lifetime.csv ./skew_test/1/lifetime-$var.csv
	cp ./skew-pt1/test02-2-task/taskset-$var/lifetime.csv ./skew_test/2/lifetime-$var.csv
	cp ./skew-pt1/test02-3-task/taskset-$var/lifetime.csv ./skew_test/3/lifetime-$var.csv
	cp ./skew-pt1/test02-4-task/taskset-$var/lifetime.csv ./skew_test/4/lifetime-$var.csv
done
for var in $(seq 13 25)
do
	cp ./skew-pt2/test02-1-task/taskset-$var/lifetime.csv ./skew_test/1/lifetime-$var.csv
	cp ./skew-pt2/test02-2-task/taskset-$var/lifetime.csv ./skew_test/2/lifetime-$var.csv
	cp ./skew-pt2/test02-3-task/taskset-$var/lifetime.csv ./skew_test/3/lifetime-$var.csv
	cp ./skew-pt2/test02-4-task/taskset-$var/lifetime.csv ./skew_test/4/lifetime-$var.csv

done
for var in $(seq 26 37)
do
	cp ./skew-pt3/test02-1-task/taskset-$var/lifetime.csv ./skew_test/1/lifetime-$var.csv
	cp ./skew-pt3/test02-2-task/taskset-$var/lifetime.csv ./skew_test/2/lifetime-$var.csv
	cp ./skew-pt3/test02-3-task/taskset-$var/lifetime.csv ./skew_test/3/lifetime-$var.csv
	cp ./skew-pt3/test02-4-task/taskset-$var/lifetime.csv ./skew_test/4/lifetime-$var.csv

done
for var in $(seq 38 50)
do 	
	cp ./skew-pt4/test02-1-task/taskset-$var/lifetime.csv ./skew_test/1/lifetime-$var.csv
	cp ./skew-pt4/test02-2-task/taskset-$var/lifetime.csv ./skew_test/2/lifetime-$var.csv
	cp ./skew-pt4/test02-3-task/taskset-$var/lifetime.csv ./skew_test/3/lifetime-$var.csv
	cp ./skew-pt4/test02-4-task/taskset-$var/lifetime.csv ./skew_test/4/lifetime-$var.csv

done

