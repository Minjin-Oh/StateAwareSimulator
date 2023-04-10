cd /home/user/StateAwareSimulator/test_shells

mkdir ./randloc-res

for var in $(seq 1 50)
do
	cp ./randloc-$var/lifetime.csv ./randloc-res/lifetime-$var.csv
	echo "\n" >> ./randloc-res/lifetime-$var.csv
	cd ./randloc-res
	cat ./lifetime-$var.csv >> loc-tot.csv
	cd ../
done
