for var in $(seq 1 5) 
	do
	mkdir ./$var-res
	cp ./taskset/$var/taskparam.csv ./
	for n in $(seq 0 3)
	do
		cp ./taskset/$var/wr_t$n.csv ./
		cp ./taskset/$var/rd_t$n.csv ./
	done
	../../statesimul.out NO NO SKIPRR nogen 4 0.3 -1 0.05 0.95
	mv ./*.csv ./$var-res
done
