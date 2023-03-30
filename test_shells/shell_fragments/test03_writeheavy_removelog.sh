cd /home/user/StateAwareSimulator/test_shells

for var in $(seq 1 40)
do
	rm -r ./writeheavy-$var/*.csv
done

