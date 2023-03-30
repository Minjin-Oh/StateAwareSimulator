#generate task and workload
for var in $(seq 1 8)
do
	mkdir ./task03-$var
	cp ./gen-tsk.sh ./task03-$var/
	cd ./task03-$var
	sh ./gen-tsk.sh
	cd ../
	sleep 1
done

#copy workload and task to each folder
for var in $(seq 1 8)
do
	for a in $(seq 1 7)
	do
		mkdir ./task03-$var-$a
		cp ./type$a.sh ./task03-$var-$a
		cp ./task03-$var/taskparam.csv ./task03-$var-$a
		for tnum in $(seq 0 3)
		do
			cp ./task03-$var/wr_t$tnum.csv ./task03-$var-$a
			cp ./task03-$var/rd_t$tnum.csv ./task03-$var-$a
		done
	done
done

#run each fragment
for var in $(seq 1 8)
do
	for a in $(seq 1 7)
	do
		cd ./task03-$var-$a
		nohup sh ./type$a.sh &
		cd ../
	done
done



