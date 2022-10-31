mkdir loc_lifetime
mkdir loc_lifetime/test02_5050
mkdir loc_lifetime/test02_6040
mkdir loc_lifetime/test02_7030
mkdir loc_lifetime/test02_8020
mkdir loc_lifetime/test02_9010
mkdir loc_lifetime/test02_9505

for var in $(seq 5)
do
	cp ./loc02-pt$var/test02-5050/*.csv ./loc_lifetime/test02_5050/
	cp ./loc02-pt$var/test02-6040/*.csv ./loc_lifetime/test02_6040/
	cp ./loc02-pt$var/test02-7030/*.csv ./loc_lifetime/test02_7030/
	cp ./loc02-pt$var/test02-8020/*.csv ./loc_lifetime/test02_8020/
	cp ./loc02-pt$var/test02-9010/*.csv ./loc_lifetime/test02_9010/
	cp ./loc02-pt$var/test02-9505/*.csv ./loc_lifetime/test02_9505/
done

