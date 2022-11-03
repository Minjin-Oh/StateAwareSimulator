mkdir ./test02-1-task
mkdir ./test02-2-task
mkdir ./test02-3-task
mkdir ./test02-4-task
for var in $(seq 13 25)
do
	for var2 in $(seq 4)
	do
		../../statesimul.out NO NO NO TASKGEN 4 0.2 0 0.05 0.95 $var2
		../../statesimul.out NO NO NO WORKGEN 4 0.2 -1 0.05 0.95
		../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
		../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
		../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
		../../statesimul.out NO NO BASE005 nogen 4 0.2 -1 -1 -1
		rm rd_t*.csv
		rm wr_t*.csv
		mkdir ./test02-$var2-task/taskset-$var
		mv ./*.csv ./test02-$var2-task/taskset-$var
	done
done
