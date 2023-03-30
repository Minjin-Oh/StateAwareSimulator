#go to test folder
cd /home/user/StateAwareSimulator/grad_test_shells

#for each tasksetgroup, make seperate folder
for var in $(seq 1 8)
do
	mkdir ./group$var-res
	for rep in $(seq 1 5)
	do
		mkdir ./group$var-res/$rep
		for a in $(seq 1 7)
		do
			cp ./newtask03-$var-$a/$rep-res/lifetime.csv ./group$var-res/$rep/lifetime-$a.csv
		done
		cat ./group$var-res/$rep/lifetime*.csv > ./group$var-res/$rep/res.csv
		echo "\n" >> ./group$var-res/$rep/res.csv
	done
	
done

for var in $(seq 1 8)
do
	for rep in $(seq 1 5)
	do
		cat ./group$var-res/$rep/res.csv >> ./task03-tot.csv
	done
done


