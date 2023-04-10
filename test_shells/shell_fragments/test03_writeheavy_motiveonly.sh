cd /home/user/StateAwareSimulator/test_shells

for var in $(seq 1 40)
do
	cp ./shell_fragments/writeheavy-motiveonly.sh ./writeheavy-$var
	cd ./writeheavy-$var
	nohup sh ./writeheavy-motiveonly.sh &
	cd ../
done
