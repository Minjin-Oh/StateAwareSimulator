#!/bin/bash

for var in $(seq 5)
do
	./statesimul.out NO NO NO WORKGEN
	./statesimul.out DOGC DOW DORR nogen
	./statesimul.out NO DOW DORR nogen
	./statesimul.out DOGC NO DORR nogen
	./statesimul.out DOGC DOW NO nogen
	./statesimul.out NO NO DORR nogen
	./statesimul.out NO DOW NO nogen
	./statesimul.out DOGC NO NO nogen
	./statesimul.out NO NO NO nogen
done
