cd /home/user/StateAwareSimulator/test_shells

for var in $(seq 1 10)
do
	cp ./shell_fragments/loc-type1-motiveonly.sh ./loc-$var/
	cd ./loc-$var
	nohup sh ./loc-type1-motiveonly.sh &
	cd ../
done

for var in $(seq 11 20)
do
	cp ./shell_fragments/loc-type2-motiveonly.sh ./loc-$var/
	cd ./loc-$var
	nohup sh ./loc-type2-motiveonly.sh &
	cd ../
done

for var in $(seq 21 30)
do
	cp ./shell_fragments/loc-type3-motiveonly.sh ./loc-$var/
	cd ./loc-$var
	nohup sh ./loc-type3-motiveonly.sh &
	cd ../
done

for var in $(seq 31 40)
do
	cp ./shell_fragments/loc-type4-motiveonly.sh ./loc-$var/
	cd ./loc-$var
	nohup sh ./loc-type4-motiveonly.sh &
	cd ../
done

for var in $(seq 41 50)
do
	cp ./shell_fragments/loc-type5-motiveonly.sh ./loc-$var/
	cd ./loc-$var
	nohup sh ./loc-type5-motiveonly.sh &
	cd ../
done

