cd /home/user/StateAwareSimulator/test_shells

for var in $(seq 1 10)
do
	mkdir ./randcyc-$var
	cp ./shell_fragments/randcyc.sh ./randcyc-$var/
	cd ./randcyc-$var
	nohup sh ./randcyc.sh &
	sleep 1
	cd ../
done

for var in $(seq 11 20)
do
	mkdir ./randcyc-$var
	cp ./shell_fragments/randcyc.sh ./randcyc-$var
	cd ./randcyc-$var
	nohup sh ./randcyc.sh &
	sleep 1
	cd ../
done

for var in $(seq 21 30)
do
	mkdir ./randcyc-$var	
	cp ./shell_fragments/randcyc.sh ./randcyc-$var
	cd ./randcyc-$var
	nohup sh ./randcyc.sh &
	sleep 1
	cd ../

done

for var in $(seq 31 40)
do
	mkdir ./randcyc-$var
	cp ./shell_fragments/randcyc.sh ./randcyc-$var
	cd ./randcyc-$var
	nohup sh ./randcyc.sh &
	sleep 1
	cd ../

done
