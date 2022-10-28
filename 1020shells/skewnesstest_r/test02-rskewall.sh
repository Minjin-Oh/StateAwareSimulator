for var in $(seq 4)
do
	../../statesimul.out NO NO NO TASKGEN 4 0.2 1 0.05 0.95 $var
	../../statesimul.out NO NO NO WORKGEN 4 0.2 -1 0.05 0.95
	../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO GREEDYW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO DOW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO HOTW SKIPRR nogen 4 0.2 -1 -1 -1 
	../../statesimul.out YOUNGGC NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out DOGC NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out YOUNGGC GREEDYW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out DOGC DOW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
	mkdir ./test02-rskew$var
	mv ./*.csv ./test02-rskew$var
done
