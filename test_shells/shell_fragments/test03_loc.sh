cd /home/user/StateAwareSimulator/test_shells

#generate 10 tasksets
for var in $(seq 1 10)
do
	mkdir ./loc-$var
	cp ./shell_fragments/loc-gentask.sh ./loc-$var/
	cd ./loc-$var
	sh ./loc-gentask.sh
	sleep 1
	cd ../
done

for var in $(seq 1 10)
do
	cp ./shell_fragments/loc-type1.sh ./loc-$var/
	cd ./loc-$var
	nohup sh ./loc-type1.sh &
	sleep 1
	cd ../
done

for var in $(seq 11 20)
do
	var2=$((var - 10))
	mkdir ./loc-$var
	cp ./shell_fragments/loc-type2.sh ./loc-$var/
	cp ./loc-$var2/taskparam.csv ./loc-$var
	cd ./loc-$var
	nohup sh ./loc-type2.sh &
	sleep 1
	cd ../
done

for var in $(seq 21 30)
do
	var2=$((var - 20))
	mkdir ./loc-$var
	cp ./shell_fragments/loc-type3.sh ./loc-$var/
	cp ./loc-$var2/taskparam.csv ./loc-$var
	cd ./loc-$var
	nohup sh ./loc-type3.sh &
	sleep 1
	cd ../
done

for var in $(seq 31 40)
do
	var2=$((var - 30))
	mkdir ./loc-$var
	cp ./shell_fragments/loc-type4.sh ./loc-$var/
	cp ./loc-$var2/taskparam.csv ./loc-$var
	cd ./loc-$var
	nohup sh ./loc-type4.sh &
	sleep 1
	cd ../
done

for var in $(seq 41 50)
do
	var2=$((var - 40))
	mkdir ./loc-$var
	cp ./shell_fragments/loc-type5.sh ./loc-$var/
	cp ./loc-$var2/taskparam.csv ./loc-$var/
	cd ./loc-$var
	nohup sh ./loc-type5.sh &
	sleep 1
	cd ../
done

:<<'END'
for var in $(seq 11 20)
do
	mkdir ./writeheavy-$var
	cp ./shell_fragments/writeheavy-type2.sh ./writeheavy-$var
	cd ./writeheavy-$var
	nohup sh ./writeheavy-type2.sh &
	sleep 1
	cd ../
done

for var in $(seq 21 30)
do
	mkdir ./writeheavy-$var	
	cp ./shell_fragments/writeheavy-type3.sh ./writeheavy-$var
	cd ./writeheavy-$var
	nohup sh ./writeheavy-type3.sh &
	sleep 1
	cd ../

done

for var in $(seq 31 40)
do
	mkdir ./writeheavy-$var
	cp ./shell_fragments/writeheavy-type4.sh ./writeheavy-$var
	cd ./writeheavy-$var
	nohup sh ./writeheavy-type4.sh &
	sleep 1
	cd ../

done
END

