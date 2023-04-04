cd /home/user/StateAwareSimulator/grad_test_shells

#for X times,
#generate task and workload
for var in $(seq 1 5)
do
	mkdir ./task03-set$1-$var
	cp ./shell_fragments/gen-tsk.sh ./task03-set$1-$var/
	cd ./task03-set$1-$var
	sh ./gen-tsk.sh
	cd ../
	
	for var in $(seq 1 7)
	
