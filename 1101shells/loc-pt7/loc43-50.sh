mkdir ./test02-9505
mkdir ./test02-9010
mkdir ./test02-8020
mkdir ./test02-7030
mkdir ./test02-6040
mkdir ./test02-5050
for var in $(seq 43 50)
do
	../../statesimul.out NO NO NO TASKGEN 4 0.2 -1 0.05 0.95
	../../statesimul.out NO NO NO WORKGEN 4 0.2 -1 0.05 0.95
	../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.2 -1 -1 -1
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test02-9505/loc-$var
	mv ./*.csv ./test02-9505/loc-$var
	
	cp ./test02-9505/loc-$var/taskparam.csv ./
	../../statesimul.out NO NO NO WORKGEN 4 0.2 -1 0.10 0.90
	../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.2 -1 -1 -1	
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test02-9010/loc-$var
	mv ./*.csv ./test02-9010/loc-$var

	cp ./test02-9505/loc-$var/taskparam.csv ./
	../../statesimul.out NO NO NO WORKGEN 4 0.2 -1 0.20 0.80
	../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.2 -1 -1 -1	
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test02-8020/loc-$var
	mv ./*.csv ./test02-8020/loc-$var
	
	cp ./test02-9505/loc-$var/taskparam.csv ./
	../../statesimul.out NO NO NO WORKGEN 4 0.2 -1 0.30 0.70
	../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.2 -1 -1 -1	
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test02-7030/loc-$var
	mv ./*.csv ./test02-7030/loc-$var

	cp ./test02-9505/loc-$var/taskparam.csv ./
	../../statesimul.out NO NO NO WORKGEN 4 0.2 -1 0.40 0.60
	../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.2 -1 -1 -1	
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test02-6040/loc-$var
	mv ./*.csv ./test02-6040/loc-$var

	
	cp ./test02-9505/loc-$var/taskparam.csv ./
	../../statesimul.out NO NO NO WORKGEN 4 0.2 -1 0.50 0.50
	../../statesimul.out NO NO SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out YOUNGGC OLDW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out UTILGC HOTW SKIPRR nogen 4 0.2 -1 -1 -1
	../../statesimul.out NO NO BASE005 nogen 4 0.2 -1 -1 -1	
	rm rd_t*.csv
	rm wr_t*.csv
	mkdir ./test02-5050/loc-$var
	mv ./*.csv ./test02-5050/loc-$var
done	

