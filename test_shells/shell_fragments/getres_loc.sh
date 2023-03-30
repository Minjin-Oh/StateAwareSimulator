cd /home/user/StateAwareSimulator/test_shells

mkdir ./loc-res

for var in $(seq 1 50)
do
	cp ./loc-$var/lifetime.csv ./loc-res/lifetime-$var.csv
	echo "\n" >> ./loc-res/lifetime-$var.csv
	cd ./loc-res
	cat ./lifetime-$var.csv >> loc-tot.csv
	cd ../
done

