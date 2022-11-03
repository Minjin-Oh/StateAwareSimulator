for var in $(seq 1)
do
	../../statesimul.out NO NO NO TASKGEN 4 0.2 0 0.05 0.95 3
	../../statesimul.out NO NO NO WORKGEN 4 0.3 -1 0.05 0.95
	../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
#	../../statesimul.out NO OLDW SKIPRR nogen 4 0.2 -1 -1 -1
#	../../statesimul.out NO HOTW SKIPRR nogen 4 0.2 -1 -1 -1
#	../../statesimul.out YOUNGGC NO SKIPRR nogen 4 0.2 -1 -1 -1
#	../../statesimul.out UTILGC NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.2 -1 -1 -1
	mkdir ./test03-single-$var
	mv ./*.csv ./test03-single-$var
done
