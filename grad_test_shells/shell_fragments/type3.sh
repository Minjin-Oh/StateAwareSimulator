for var in $(seq 1 5) 
do
	mkdir ./$var-res
	cp ./$var/taskparam.csv ./
	for n in $(seq 0 3)
	do
		cp ./$var/wr_t$n.csv ./
		cp ./$var/rd_t$n.csv ./
	done
	../../statesimul.out NO NO BASE005 nogen 4 0.3 -1 0.05 0.95
	mv ./*.csv ./$var-res
done
