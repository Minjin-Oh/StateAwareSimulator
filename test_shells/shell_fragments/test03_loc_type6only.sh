cd /home/user/StateAwareSimulator/test_shells

#test only type6 (per-task locality assignment)

for var in $(seq 1 10)
do
	var2=$((var))
	mkdir ./randloc-$var
	cp ./shell_fragments/loc-type6.sh ./randloc-$var/
	cp ./loc-$var2/taskparam.csv ./randloc-$var
	cd ./randloc-$var
	nohup sh ./loc-type6.sh &
	sleep 1
	cd ../
done

for var in $(seq 11 20)
do
	var2=$((var - 10))
	mkdir ./randloc-$var
	cp ./shell_fragments/loc-type6.sh ./randloc-$var/
	cp ./loc-$var2/taskparam.csv ./randloc-$var
	cd ./randloc-$var
	nohup sh ./loc-type6.sh &
	sleep 1
	cd ../
done

for var in $(seq 21 30)
do
	var2=$((var - 20))
	mkdir ./randloc-$var
	cp ./shell_fragments/loc-type6.sh ./randloc-$var/
	cp ./loc-$var2/taskparam.csv ./randloc-$var
	cd ./randloc-$var
	nohup sh ./loc-type6.sh &
	sleep 1
	cd ../
done

for var in $(seq 31 40)
do
	var2=$((var - 30))
	mkdir ./randloc-$var
	cp ./shell_fragments/loc-type6.sh ./randloc-$var/
	cp ./loc-$var2/taskparam.csv ./randloc-$var
	cd ./randloc-$var
	nohup sh ./loc-type6.sh &
	sleep 1
	cd ../
done
