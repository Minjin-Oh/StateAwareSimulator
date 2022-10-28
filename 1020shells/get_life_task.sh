mkdir ./lifetimes
mkdir ./lifetimes/local02
mkdir ./lifetimes/local03
mkdir ./lifetimes/skew_r
mkdir ./lifetimes/skew_w
mkdir ./lifetimes/util01
mkdir ./lifetimes/util02
mkdir ./lifetimes/util03

mkdir ./taskparams
mkdir ./taskparams/local02
mkdir ./taskparams/local03
mkdir ./taskparams/skew_r
mkdir ./taskparams/skew_w
mkdir ./taskparams/util01
mkdir ./taskparams/util02
mkdir ./taskparams/util03

cp ./loc02/test02-5050/lifetime.csv ./lifetimes/local02/lifetime-5050.csv
cp ./loc02/test02-6040/lifetime.csv ./lifetimes/local02/lifetime-6040.csv
cp ./loc02/test02-7030/lifetime.csv ./lifetimes/local02/lifetime-7030.csv
cp ./loc02/test02-8020/lifetime.csv ./lifetimes/local02/lifetime-8020.csv
cp ./loc02/test02-9010/lifetime.csv ./lifetimes/local02/lifetime-9010.csv
cp ./loc02/test02-9505/lifetime.csv ./lifetimes/local02/lifetime-9505.csv

cp ./loc03/test03-5050/lifetime.csv ./lifetimes/local03/lifetime-5050.csv
cp ./loc03/test03-6040/lifetime.csv ./lifetimes/local03/lifetime-6040.csv
cp ./loc03/test03-7030/lifetime.csv ./lifetimes/local03/lifetime-7030.csv
cp ./loc03/test03-8020/lifetime.csv ./lifetimes/local03/lifetime-8020.csv
cp ./loc03/test03-9010/lifetime.csv ./lifetimes/local03/lifetime-9010.csv
cp ./loc03/test03-9505/lifetime.csv ./lifetimes/local03/lifetime-9505.csv

for var in $(seq 4)
do
	cp ./skewnesstest_r/test02-rskew$var/lifetime.csv ./lifetimes/skew_r/lifetime-rs$var.csv
done
for var in $(seq 4)
do
	cp ./skewnesstest_w/test02-wskew$var/lifetime.csv ./lifetimes/skew_w/lifetime-ws$var.csv
done

for var in $(seq 50)
do
	cp ./utest01/test01-$var/lifetime.csv ./lifetimes/util01/lifetime-$var.csv
	cp ./utest02/test02-$var/lifetime.csv ./lifetimes/util02/lifetime-$var.csv
	cp ./utest03/test03-$var/lifetime.csv ./lifetimes/util03/lifetime-$var.csv
done

cp ./loc02/test02-5050/taskparam.csv ./taskparams/local02/taskparam-5050.csv
cp ./loc02/test02-6040/taskparam.csv ./taskparams/local02/taskparam-6040.csv
cp ./loc02/test02-7030/taskparam.csv ./taskparams/local02/taskparam-7030.csv
cp ./loc02/test02-8020/taskparam.csv ./taskparams/local02/taskparam-8020.csv
cp ./loc02/test02-9010/taskparam.csv ./taskparams/local02/taskparam-9010.csv
cp ./loc02/test02-9505/taskparam.csv ./taskparams/local02/taskparam-9505.csv

cp ./loc03/test03-5050/taskparam.csv ./taskparams/local03/taskparam-5050.csv
cp ./loc03/test03-6040/taskparam.csv ./taskparams/local03/taskparam-6040.csv
cp ./loc03/test03-7030/taskparam.csv ./taskparams/local03/taskparam-7030.csv
cp ./loc03/test03-8020/taskparam.csv ./taskparams/local03/taskparam-8020.csv
cp ./loc03/test03-9010/taskparam.csv ./taskparams/local03/taskparam-9010.csv
cp ./loc03/test03-9505/taskparam.csv ./taskparams/local03/taskparam-9505.csv

for var in $(seq 4)
do
	cp ./skewnesstest_r/test02-rskew$var/taskparam.csv ./taskparams/skew_r/taskparam-rs$var.csv
done
for var in $(seq 4)
do
	cp ./skewnesstest_w/test02-wskew$var/taskparam.csv ./taskparams/skew_w/taskparam-ws$var.csv
done

for var in $(seq 50)
do
	cp ./utest01/test01-$var/taskparam.csv ./taskparams/util01/taskparam-$var.csv
	cp ./utest02/test02-$var/taskparam.csv ./taskparams/util02/taskparam-$var.csv
	cp ./utest03/test03-$var/taskparam.csv ./taskparams/util03/taskparam-$var.csv
done
