cd ./rand_test/u01
sh ./agg.sh
cd ../u015
sh ./agg.sh
cd ../u02
sh ./agg.sh
cd ../u025
sh ./agg.sh
cd ../u03
sh ./agg.sh
cd ../u035
sh ./agg.sh

cd ../
cp ./u01/result.csv ./result-01.csv
cp ./u015/result.csv ./result-015.csv
cp ./u02/result.csv ./result-02.csv
cp ./u025/result.csv ./result-025.csv
cp ./u03/result.csv ./result-03.csv
cp ./u035/result.csv ./result-035.csv
