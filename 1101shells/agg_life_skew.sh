cd ./skew_test/1
sh ./agg.sh
cd ../2
sh ./agg.sh
cd ../3
sh ./agg.sh
cd ../4
sh ./agg.sh

cd ../
cp ./1/result.csv ./result-1.csv
cp ./2/result.csv ./result-2.csv
cp ./3/result.csv ./result-3.csv
cp ./4/result.csv ./result-4.csv
