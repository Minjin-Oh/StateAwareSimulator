#go to test folder
cd /home/user/StateAwareSimulator/grad_test_shells

#for each tasksetgroup, make seperate folder
mkdir ./tasksets
for var in $(seq 1 8)
do
	for rep in $(seq 1 5)
	do
			cp ./newtask03-$var/$rep/taskparam.csv ./tasksets/taskparam-$var-$rep.csv
	done
done
