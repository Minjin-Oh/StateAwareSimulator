mkdir ./u01_test
mkdir ./u015_test
mkdir ./u035_test

num=0

for var in $(seq 1 10)
do
	for var2 in $(seq 1 5)
	do
		num=$(($num+1))	
		cp ./rand01-pt$var/test01-$var2/lifetime.csv ./u01_test/lifetime-$num.csv
		cp ./rand015-pt$var/test015-$var2/lifetime.csv ./u015_test/lifetime-$num.csv
		cp ./rand035-pt$var/test035-$var2/lifetime.csv ./u035_test/lifetime-$num.csv
	done
done
