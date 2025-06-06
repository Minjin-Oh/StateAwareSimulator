### program execution flag explanation
### ./statesimul.out W_POLICY GC_POLICY RR_POLICY TASKNUM TASKUTIL SKEWFLAG T_LOC S_LOC SKEWNUM INITCYC

./statesimul.out NO NO NO TASKGEN 4 0.3 -1 -2.0 -2.0 0
./statesimul.out NO NO NO WORKGEN 4 0.3 -1 -1.0 -1.0 0
### mkdir ./timing
cd ./timing
find . -maxdepth 1 -name '*.csv' -delete
cd ../
./statesimul.out NO NO SKIPRR PROFGEN 4 0.3 -1 -2.0 -2.0 -1
#./statesimul.out NO NO SKIPRR SCATTERGEN 4 0.2 0 -2.0 -2.0 4

./statesimul.out NO NO SKIPRR nogen 4 0.3 -1 -2.0 -2.0 0
#./statesimul.out NO INVW SKIPRR nogen 4 0.3 -1 -1.0 -1.0 -1
#./statesimul.out NO GRADW SKIPRR nogen 4 0.3 -1 0.5 0.5 -1
#./statesimul.out NO MOTIVALLY BASE005 nogen 4 0.3 -1 0.5 0.5 -1


#./statesimul.out NO OLDW SKIPRR nogen 4 0.2 -1 -1 -1
#./statesimul.out NO HOTW SKIPRR nogen 4 0.2 -1 -1 -1
#./statesimul.out YOUNGGC NO SKIPRR nogen 4 0.2 -1 -1 -1
#./statesimul.out UTILGC NO SKIPRR nogen 4 0.2 -1 -1 -1
#./statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
#./statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1

#./statesimul.out NO NO SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1
#./statesimul.out NO GRADW SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1
#./statesimul.out NO GRADW_MOD SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1
#./statesimul.out NO HOTW SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1

#./statesimul.out NO MOTIVALLY SKIPRR nogen 4 0.3 -1 0.3 0.7 -1 -1

#./statesimul.out UTILGC NO SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1
#./statesimul.out UTILGC INVW SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1
#./statesimul.out UTILGC GRADW SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1

#./statesimul.out UTILGC GRADW_MOD SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1
#./statesimul.out UTILGC HOTW SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1
#./statesimul.out UTILGC MOTIVALLY SKIPRR nogen 4 0.3 -1 0.05 0.95 -1 -1

#./statesimul.out UTILGC NO RR005 nogen 4 0.3 -1 0.05 0.95 -1 -1
#./statesimul.out UTILGC GRADW RR005 nogen 4 0.3 -1 0.05 0.95 -1 -1

#./statesimul.out UTILGC GRADW RR005 nogen 4 0.3 -1 0.05 0.95 -1 -1

#./statesimul.out UTILGC HOTW RR005 nogen 4 0.3 -1 0.05 0.95 -1 -1
#./statesimul.out UTILGC MOTIVALLY RR005 nogen 4 0.3 -1 0.05 0.95 -1 -1


#./statesimul.out UTILGC NO RR005 nogen 4 0.2 -1 0.05 0.95
#./statesimul.out UTILGC GRADW RR005 nogen 4 0.2 -1 0.05 0.95
#./statesimul.out UTILGC HOTW RR005 nogen 4 0.2 -1 0.05 0.95
#./statesimul.out UTILGC MOTIVWO RR005 nogen 4 0.2 -1 0.05 0.95

#./statesimul.out NO NO BASE005 nogen 4 0.3 -1 0.05 0.95 -1 -1

#./statesimul.out NO MOTIVALLY BASE005 nogen 4 0.3 -1 0.3 0.7 -1 -1

##mkdir ./testres
#mv ./*.csv ./testres

