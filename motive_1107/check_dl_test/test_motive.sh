#../../statesimul.out NO NO NO TASKGEN 3 0.15 -2 0.05 0.95
#../../statesimul.out NO NO NO WORKGEN 3 0.15 -2 0.05 0.95
../../statesimul.out NO NO SKIPRR nogen 3 0.15 -1 -1 -1 -1 350
#./statesimul.out NO OLDW SKIPRR nogen 4 0.2 -1 -1 -1
#./statesimul.out NO HOTW SKIPRR nogen 4 0.2 -1 -1 -1
#./statesimul.out YOUNGGC NO SKIPRR nogen 4 0.2 -1 -1 -1
#./statesimul.out UTILGC NO SKIPRR nogen 4 0.2 -1 -1 -1
../../statesimul.out YOUNGGC GREEDYW SKIPRR nogen 3 0.2 -1 -1 -1 -1 350
../../statesimul.out UTILGC HOTW RR005 nogen 3 0.2 -1 -1 -1 -1 350
../../statesimul.out NO NO BASE005 nogen 3 0.2 -1 -1 -1 -1 350
#mkdir ./testres
#mv ./*.csv ./testres

