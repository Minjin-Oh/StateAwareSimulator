cd /home/user/StateAwareSimulator/test_shells

for var in $(seq 1 40)
do
	mkdir ./randcyc_base-$var
	cp ./randcyc-$var/taskparam.csv ./randcyc_base-$var/
	for tnum in $(seq 0 3)
	do
		cp ./randcyc-$var/wr_t$tnum.csv ./randcyc_base-$var/
		cp ./randcyc-$var/rd_t$tnum.csv ./randcyc_base-$var/
	done
	cp ./shell_fragments/randcyc_base.sh ./randcyc_base-$var
	cd ./randcyc_base-$var
	nohup sh ./randcyc_base.sh &
	sleep 1
	cd ../
done
:<<"END"
for var in $(seq 11 20)
do
	mkdir ./randcyc_base-$var
	cp ./shell_fragments/randcyc_base.sh ./randcyc_base-$var
	for tnum in $(seq 0 3)
	do
		cp ./randcyc-$var/wr_t$tnum.csv ./randcyc_base-$var/
		cp ./randcyc-$var/rd_t$tnum.csv ./randcyc_base-$var/
	done
	cd ./randcyc_base-$var
	nohup sh ./randcyc_base.sh &
	sleep 1
	cd ../
done

for var in $(seq 21 30)
do
	mkdir ./randcyc_base-$var	
	cp ./shell_fragments/randcyc_base.sh ./randcyc_base-$var
	for tnum in $(seq 0 3)
	do
		cp ./randcyc-$var/wr_t$tnum.csv ./randcyc_base-$var/
		cp ./randcyc-$var/rd_t$tnum.csv ./randcyc_base-$var/
	done
	cd ./randcyc_base-$var
	nohup sh ./randcyc_base.sh &
	sleep 1
	cd ../

done

for var in $(seq 31 40)
do
	mkdir ./randcyc_base-$var
	cp ./shell_fragments/randcyc_base.sh ./randcyc_base-$var
	for tnum in $(seq 0 3)
	do
		cp ./randcyc-$var/wr_t$tnum.csv ./randcyc_base-$var/
		cp ./randcyc-$var/rd_t$tnum.csv ./randcyc_base-$var/
	done
	cd ./randcyc_base-$var
	nohup sh ./randcyc_base.sh &
	sleep 1
	cd ../

done
END
