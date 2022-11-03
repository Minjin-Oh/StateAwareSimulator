for var in $(seq 1 7)
do
	../../statesimul.out NO NO NO TASKGEN 4 0.3 -1 0.05 0.95
	../../statesimul.out NO NO NO WORKGEN 4 0.3 -1 0.05 0.95
	../../statesimul.out NO NO SKIPRR nogen 4 0.3 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.3 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.3 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.3 -1 -1 -1
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test03-$var
	mv ./*.csv ./test03-$var

	
	../../statesimul.out NO NO NO TASKGEN 4 0.25 -1 0.05 0.95
	../../statesimul.out NO NO NO WORKGEN 4 0.25 -1 0.05 0.95
	../../statesimul.out NO NO SKIPRR nogen 4 0.25 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.25 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.25 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.25 -1 -1 -1
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test025-$var
	mv ./*.csv ./test025-$var
	
	../../statesimul.out NO NO NO TASKGEN 4 0.2 -1 0.05 0.95
	../../statesimul.out NO NO NO WORKGEN 4 0.2 -1 0.05 0.95
	../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.2 -1 -1 -1
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test02-$var
	mv ./*.csv ./test02-$var

	../../statesimul.out NO NO NO TASKGEN 4 0.15 -1 0.05 0.95
	../../statesimul.out NO NO NO WORKGEN 4 0.15 -1 0.05 0.95
	../../statesimul.out NO NO SKIPRR nogen 4 0.15 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.15 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.15 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.15 -1 -1 -1
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test015-$var
	mv ./*.csv ./test015-$var

	../../statesimul.out NO NO NO TASKGEN 4 0.1 -1 0.05 0.95
	../../statesimul.out NO NO NO WORKGEN 4 0.1 -1 0.05 0.95
	../../statesimul.out NO NO SKIPRR nogen 4 0.1 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.1 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.1 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.1 -1 -1 -1
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test01-$var
	mv ./*.csv ./test01-$var
done



