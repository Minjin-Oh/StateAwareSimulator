cd /home/techyee/RealtimeFlash/StateAwareSimulator/test_shells

mkdir ./task03
cd ./task03
cp ../shell_fragments/gen-tsk.sh ./
sh ./gen-tsk.sh
cd ../

for a in $(seq 1 5)
do
	mkdir ./task03-$a
	cp ./task03/taskparam.csv ./task03-$a
	cp ./shell_fragments/type$a.sh ./task03-$a
	for tnum in $(seq 0 3)
	do
		cp ./task03/wr_t$tnum.csv ./task03-$a/
		cp ./task03/rd_t$tnum.csv ./task03-$a/
	done
done

for a in $(seq 1 7)
do
	cd ./task03-$a
	nohup sh ./type$a.sh &
	cd ../
done
