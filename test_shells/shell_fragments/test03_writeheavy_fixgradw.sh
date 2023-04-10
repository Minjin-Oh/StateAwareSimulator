cd /home/user/StateAwareSimulator/test_shells

for var in $(seq 1 10)
do
	mkdir ./writeheavy-$var
	cp ./shell_fragments/writeheavy-type1.sh ./writeheavy-$var/
	cd ./writeheavy-$var
	mkdir ./oldres
	mv *.csv ./oldres
	cd ./oldres
	mv ./wr*.csv ../
	mv ./rd*.csv ../
	mv ./taskparam.csv ../
	mv ./loc.csv ../
	cd ../
	nohup sh ./writeheavy-type1.sh &
	sleep 1
	cd ../
done

for var in $(seq 11 20)
do
	mkdir ./writeheavy-$var
	cp ./shell_fragments/writeheavy-type2.sh ./writeheavy-$var
	cd ./writeheavy-$var
	mkdir ./oldres
	mv *.csv ./oldres
	cd ./oldres
	mv ./wr*.csv ../
	mv ./rd*.csv ../
	mv ./taskparam.csv ../
	mv ./loc.csv ../
	cd ../
	nohup sh ./writeheavy-type2.sh &
	sleep 1
	cd ../
done

for var in $(seq 21 30)
do
	mkdir ./writeheavy-$var	
	cp ./shell_fragments/writeheavy-type3.sh ./writeheavy-$var
	cd ./writeheavy-$var
	mkdir ./oldres
	mv *.csv ./oldres
	cd ./oldres
	mv ./wr*.csv ../
	mv ./rd*.csv ../
	mv ./taskparam.csv ../
	mv ./loc.csv ../
	cd ../
	nohup sh ./writeheavy-type3.sh &
	sleep 1
	cd ../

done

for var in $(seq 31 40)
do
	mkdir ./writeheavy-$var
	cp ./shell_fragments/writeheavy-type4.sh ./writeheavy-$var
	cd ./writeheavy-$var
	mkdir ./oldres
	mv *.csv ./oldres
	cd ./oldres
	mv ./wr*.csv ../
	mv ./rd*.csv ../
	mv ./taskparam.csv ../
	mv ./loc.csv ../
	cd ../
	nohup sh ./writeheavy-type4.sh &
	sleep 1
	cd ../

done
