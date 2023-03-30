cd /home/user/StateAwareSimulator/test_shells

mkdir ./randcyc-base-res

for var in $(seq 1 40)
do
	cp ./randcyc_base-$var/lifetime.csv ./randcyc-base-res/lifetime-$var.csv
	echo "\n" >> ./randcyc-base-res/lifetime-$var.csv
	cd ./randcyc-base-res
	cat ./lifetime-$var.csv >> randcyc-base-tot.csv
	cd ../
done
