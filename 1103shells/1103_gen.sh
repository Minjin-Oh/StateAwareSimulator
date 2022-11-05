for var in $(seq 1 10)
do
	mkdir ./rand01-pt$var
	mkdir ./rand015-pt$var
	mkdir ./rand035-pt$var
	cp ./rand01-1-5.sh ./rand01-pt$var
	cp ./rand015-1-5.sh ./rand015-pt$var
	cp ./rand035-1-5.sh ./rand035-pt$var
done

