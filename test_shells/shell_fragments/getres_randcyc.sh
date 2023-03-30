cd /home/user/StateAwareSimulator/test_shells

mkdir ./randcyc-res

for var in $(seq 1 40)
do
	cp ./randcyc-$var/lifetime.csv ./randcyc-res/lifetime-$var.csv
	echo "\n" >> ./randcyc-res/lifetime-$var.csv
	cd ./randcyc-res
	cat ./lifetime-$var.csv >> randcyc-tot.csv
	cd ../
done
