for var in $(seq 1 10)
do
	cd ./rand01-pt$var
	nohup sh ./rand01-1-5.sh &
	cd ../rand015-pt$var
	nohup sh ./rand015-1-5.sh &
	cd ../rand035-pt$var
	nohup sh ./rand035-1-5.sh &
	cd ../
done
