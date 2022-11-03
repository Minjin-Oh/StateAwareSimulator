#!/bin/bash

mkdir ./rand_test
mkdir ./rand_test/u01
mkdir ./rand_test/u015
mkdir ./rand_test/u02
mkdir ./rand_test/u025
mkdir ./rand_test/u03
mkdir ./rand_test/u035



for var in $(seq 0 6)
do
	for num in $(seq 1 7)
	do
		let off=$var*7+$num
		let pt=$var+1
		cp ./rand-pt$pt/test01-$off/lifetime.csv ./rand_test/u01/lifetime-$off.csv
		cp ./rand-pt$pt/test015-$off/lifetime.csv ./rand_test/u015/lifetime-$off.csv
		cp ./rand-pt$pt/test02-$off/lifetime.csv ./rand_test/u02/lifetime-$off.csv
		cp ./rand-pt$pt/test025-$off/lifetime.csv ./rand_test/u025/lifetime-$off.csv
		cp ./rand-pt$pt/test03-$off/lifetime.csv ./rand_test/u03/lifetime-$off.csv
		cp ./rand-pt$pt/test035-$off/lifetime.csv ./rand_test/u035/lifetime-$off.csv
	done
done

: << 'END'
for var in $(seq 1 7)
do
	cp ./rand-pt1/test01-$var/lifetime.csv ./rand_res/u01/lifetime-$var.csv
	cp ./rand-pt1/test015-$var/lifetime.csv ./rand_res/u015/lifetime-$var.csv
	cp ./rand-pt1/test02-$var/lifetime.csv ./rand_res/u02/lifetime-$var.csv
	cp ./rand-pt1/test025-$var/lifetime.csv ./rand_res/u025/lifetime-$var.csv
	cp ./rand-pt1/test03-$var/lifetime.csv ./rand_res/u03/lifetime-$var.csv
	cp ./rand-pt1/test035-$var/lifetime.csv ./rand_res/u035/lifetime-$var.csv
done

for var in $(seq 8 14)
do
	cp ./rand-pt2/test01-$var/lifetime.csv ./rand_res/u01/lifetime-$var.csv
	cp ./rand-pt2/test015-$var/lifetime.csv ./rand_res/u015/lifetime-$var.csv
	cp ./rand-pt2/test02-$var/lifetime.csv ./rand_res/u02/lifetime-$var.csv
	cp ./rand-pt2/test025-$var/lifetime.csv ./rand_res/u025/lifetime-$var.csv
	cp ./rand-pt2/test03-$var/lifetime.csv ./rand_res/u03/lifetime-$var.csv
	cp ./rand-pt2/test035-$var/lifetime.csv ./rand_res/u035/lifetime-$var.csv
done

for var in $(seq 15 21)
do
	cp ./rand-pt3/test01-$var/lifetime.csv ./rand_res/u01/lifetime-$var.csv
	cp ./rand-pt3/test015-$var/lifetime.csv ./rand_res/u015/lifetime-$var.csv
	cp ./rand-pt3/test02-$var/lifetime.csv ./rand_res/u02/lifetime-$var.csv
	cp ./rand-pt3/test025-$var/lifetime.csv ./rand_res/u025/lifetime-$var.csv
	cp ./rand-pt3/test03-$var/lifetime.csv ./rand_res/u03/lifetime-$var.csv
	cp ./rand-pt3/test035-$var/lifetime.csv ./rand_res/u035/lifetime-$var.csv
done

for var in $(seq 22 28)
do
	cp ./rand-pt4/test01-$var/lifetime.csv ./rand_res/u01/lifetime-$var.csv
	cp ./rand-pt4/test015-$var/lifetime.csv ./rand_res/u015/lifetime-$var.csv
	cp ./rand-pt4/test02-$var/lifetime.csv ./rand_res/u02/lifetime-$var.csv
	cp ./rand-pt4/test025-$var/lifetime.csv ./rand_res/u025/lifetime-$var.csv
	cp ./rand-pt4/test03-$var/lifetime.csv ./rand_res/u03/lifetime-$var.csv
	cp ./rand-pt4/test035-$var/lifetime.csv ./rand_res/u035/lifetime-$var.csv
done

for var in $(seq 29 35)
do
	cp ./rand-pt5/test01-$var/lifetime.csv ./rand_res/u01/lifetime-$var.csv
	cp ./rand-pt5/test015-$var/lifetime.csv ./rand_res/u015/lifetime-$var.csv
	cp ./rand-pt5/test02-$var/lifetime.csv ./rand_res/u02/lifetime-$var.csv
	cp ./rand-pt5/test025-$var/lifetime.csv ./rand_res/u025/lifetime-$var.csv
	cp ./rand-pt5/test03-$var/lifetime.csv ./rand_res/u03/lifetime-$var.csv
	cp ./rand-pt5/test035-$var/lifetime.csv ./rand_res/u035/lifetime-$var.csv
done

for var in $(seq 36 42)
do
	cp ./rand-pt6/test01-$var/lifetime.csv ./rand_res/u01/lifetime-$var.csv
	cp ./rand-pt6/test015-$var/lifetime.csv ./rand_res/u015/lifetime-$var.csv
	cp ./rand-pt6/test02-$var/lifetime.csv ./rand_res/u02/lifetime-$var.csv
	cp ./rand-pt6/test025-$var/lifetime.csv ./rand_res/u025/lifetime-$var.csv
	cp ./rand-pt6/test03-$var/lifetime.csv ./rand_res/u03/lifetime-$var.csv
	cp ./rand-pt6/test035-$var/lifetime.csv ./rand_res/u035/lifetime-$var.csv
done

for var in $(seq 43 50)
do
	cp ./rand-pt7/test01-$var/lifetime.csv ./rand_res/u01/lifetime-$var.csv
	cp ./rand-pt7/test015-$var/lifetime.csv ./rand_res/u015/lifetime-$var.csv
	cp ./rand-pt7/test02-$var/lifetime.csv ./rand_res/u02/lifetime-$var.csv
	cp ./rand-pt7/test025-$var/lifetime.csv ./rand_res/u025/lifetime-$var.csv
	cp ./rand-pt7/test03-$var/lifetime.csv ./rand_res/u03/lifetime-$var.csv
	cp ./rand-pt7/test035-$var/lifetime.csv ./rand_res/u035/lifetime-$var.csv
done

