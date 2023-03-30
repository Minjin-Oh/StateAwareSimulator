cd /home/user/StateAwareSimulator/test_shells

mkdir ./writeheavy-res
for var in $(seq 1 40)
do
	cp ./writeheavy-$var/lifetime.csv ./writeheavy-res/lifetime-$var.csv
	echo "\n" >> ./writeheavy-res/lifetime-$var.csv
	cd ./writeheavy-res
	cat ./lifetime-$var.csv >> writeheavy-tot.csv
	cd ../
done

