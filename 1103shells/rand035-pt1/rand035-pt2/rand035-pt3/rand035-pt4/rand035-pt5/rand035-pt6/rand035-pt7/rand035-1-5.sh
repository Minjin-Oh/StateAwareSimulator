for var in $(seq 1 5)
do
	../../statesimul.out NO NO NO TASKGEN 4 0.35 -1 0.05 0.95
	../../statesimul.out NO NO NO WORKGEN 4 0.35 -1 0.05 0.95
	../../statesimul.out NO NO SKIPRR nogen 4 0.35 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.35 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.35 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.35 -1 -1 -1
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test035-$var
	mv ./*.csv ./test035-$var
done



