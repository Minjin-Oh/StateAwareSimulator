mkdir ./loc_test
mkdir ./loc_test/5050
mkdir ./loc_test/6040
mkdir ./loc_test/7030
mkdir ./loc_test/8020
mkdir ./loc_test/9010
mkdir ./loc_test/9505

for var in $(seq 0 6)
do
	for num in $(seq 1 7)
	do
		let off=$var*7+$num
		let pt=$var+1
		cp ./loc-pt$pt/test02-5050/loc-$off/lifetime.csv ./loc_test/5050/lifetime-$off.csv
		cp ./loc-pt$pt/test02-6040/loc-$off/lifetime.csv ./loc_test/6040/lifetime-$off.csv
		cp ./loc-pt$pt/test02-7030/loc-$off/lifetime.csv ./loc_test/7030/lifetime-$off.csv
		cp ./loc-pt$pt/test02-8020/loc-$off/lifetime.csv ./loc_test/8020/lifetime-$off.csv
		cp ./loc-pt$pt/test02-9010/loc-$off/lifetime.csv ./loc_test/9010/lifetime-$off.csv
		cp ./loc-pt$pt/test02-9505/loc-$off/lifetime.csv ./loc_test/9505/lifetime-$off.csv
	done
done

: << 'END'

for var in $(seq 8 14)
do
	cp ./loc-pt2/test02-5050/loc-$var/lifetime.csv ./loc_res/5050/lifetime-$var.csv
	cp ./loc-pt2/test02-6040/loc-$var/lifetime.csv ./loc_res/6040/lifetime-$var.csv
	cp ./loc-pt2/test02-7030/loc-$var/lifetime.csv ./loc_res/7030/lifetime-$var.csv
	cp ./loc-pt2/test02-8020/loc-$var/lifetime.csv ./loc_res/8020/lifetime-$var.csv
	cp ./loc-pt2/test02-9010/loc-$var/lifetime.csv ./loc_res/9010/lifetime-$var.csv
	cp ./loc-pt2/test02-9505/loc-$var/lifetime.csv ./loc_res/9505/lifetime-$var.csv
done

for var in $(seq 15 21)
do
	cp ./loc-pt3/test02-5050/loc-$var/lifetime.csv ./loc_res/5050/lifetime-$var.csv
	cp ./loc-pt3/test02-6040/loc-$var/lifetime.csv ./loc_res/6040/lifetime-$var.csv
	cp ./loc-pt3/test02-7030/loc-$var/lifetime.csv ./loc_res/7030/lifetime-$var.csv
	cp ./loc-pt3/test02-8020/loc-$var/lifetime.csv ./loc_res/8020/lifetime-$var.csv
	cp ./loc-pt3/test02-9010/loc-$var/lifetime.csv ./loc_res/9010/lifetime-$var.csv
	cp ./loc-pt3/test02-9505/loc-$var/lifetime.csv ./loc_res/9505/lifetime-$var.csv
done

for var in $(seq 22 28)
do
	cp ./loc-pt4/test02-5050/loc-$var/lifetime.csv ./loc_res/5050/lifetime-$var.csv
	cp ./loc-pt4/test02-6040/loc-$var/lifetime.csv ./loc_res/6040/lifetime-$var.csv
	cp ./loc-pt4/test02-7030/loc-$var/lifetime.csv ./loc_res/7030/lifetime-$var.csv
	cp ./loc-pt4/test02-8020/loc-$var/lifetime.csv ./loc_res/8020/lifetime-$var.csv
	cp ./loc-pt4/test02-9010/loc-$var/lifetime.csv ./loc_res/9010/lifetime-$var.csv
	cp ./loc-pt4/test02-9505/loc-$var/lifetime.csv ./loc_res/9505/lifetime-$var.csv
done

for var in $(seq 29 35)
do	cp ./loc-pt5/test02-5050/loc-$var/lifetime.csv ./loc_res/5050/lifetime-$var.csv
	cp ./loc-pt5/test02-6040/loc-$var/lifetime.csv ./loc_res/6040/lifetime-$var.csv
	cp ./loc-pt5/test02-7030/loc-$var/lifetime.csv ./loc_res/7030/lifetime-$var.csv
	cp ./loc-pt5/test02-8020/loc-$var/lifetime.csv ./loc_res/8020/lifetime-$var.csv
	cp ./loc-pt5/test02-9010/loc-$var/lifetime.csv ./loc_res/9010/lifetime-$var.csv
	cp ./loc-pt5/test02-9505/loc-$var/lifetime.csv ./loc_res/9505/lifetime-$var.csv

done

for var in $(seq 36 42)
do	cp ./loc-pt6/test02-5050/loc-$var/lifetime.csv ./loc_res/5050/lifetime-$var.csv
	cp ./loc-pt6/test02-6040/loc-$var/lifetime.csv ./loc_res/6040/lifetime-$var.csv
	cp ./loc-pt6/test02-7030/loc-$var/lifetime.csv ./loc_res/7030/lifetime-$var.csv
	cp ./loc-pt6/test02-8020/loc-$var/lifetime.csv ./loc_res/8020/lifetime-$var.csv
	cp ./loc-pt6/test02-9010/loc-$var/lifetime.csv ./loc_res/9010/lifetime-$var.csv
	cp ./loc-pt6/test02-9505/loc-$var/lifetime.csv ./loc_res/9505/lifetime-$var.csv

done

for var in $(seq 43 50)
do	cp ./loc-pt7/test02-5050/loc-$var/lifetime.csv ./loc_res/5050/lifetime-$var.csv
	cp ./loc-pt7/test02-6040/loc-$var/lifetime.csv ./loc_res/6040/lifetime-$var.csv
	cp ./loc-pt7/test02-7030/loc-$var/lifetime.csv ./loc_res/7030/lifetime-$var.csv
	cp ./loc-pt7/test02-8020/loc-$var/lifetime.csv ./loc_res/8020/lifetime-$var.csv
	cp ./loc-pt7/test02-9010/loc-$var/lifetime.csv ./loc_res/9010/lifetime-$var.csv
	cp ./loc-pt7/test02-9505/loc-$var/lifetime.csv ./loc_res/9505/lifetime-$var.csv
done
