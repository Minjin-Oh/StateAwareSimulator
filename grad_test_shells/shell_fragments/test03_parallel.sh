#go to test folder
cd /home/user/StateAwareSimulator/grad_test_shells


#generate task and workload
for var in $(seq 1 8)
do
	mkdir ./newtask03-$var
	cd ./newtask03-$var
	cp ../shell_fragments/gen-tsk.sh ./
	for rep in $(seq 1 5)
	do
		sh ./gen-tsk.sh
		mkdir ./$rep
		mv *.csv ./$rep
		sleep 1
	done
	cd /home/user/StateAwareSimulator/grad_test_shells
done


#copy workload and task to each folder
for var in $(seq 1 8)
do
	#for 7 different cases,
	for a in $(seq 1 7)
	do
		#copy the generated tasksets for each policy cases
		mkdir ./newtask03-$var-$a
		cp ./shell_fragments/type$a.sh ./newtask03-$var-$a
		cd ./newtask03-$var-$a
		for rep in $(seq 1 5)
		do
			mkdir ./$rep
			cp ../newtask03-$var/$rep/taskparam.csv ./$rep/
			for tnum in $(seq 0 3)
			do
				cp ../newtask03-$var/$rep/wr_t$tnum.csv ./$rep/
				cp ../newtask03-$var/$rep/rd_t$tnum.csv ./$rep/
			done
		done
		cd ../
	done
	echo ::Group $var:: tasksets are copied
done



#run each fragment
for var in $(seq 1 8)
do
	for a in $(seq 1 7)
	do
		cd ./newtask03-$var-$a
		nohup sh ./type$a.sh &
		cd ../
	done
done

