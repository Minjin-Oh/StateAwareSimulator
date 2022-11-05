for var in $(seq 1 10)
do
	cd ./rand01-pt$var
	rm -r ./test*
	cd ../rand015-pt$var
	rm -r ./test*
	cd ../rand035-pt$var
	rm -r ./test*
	cd ../
done
