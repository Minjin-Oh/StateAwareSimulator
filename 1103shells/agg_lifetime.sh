cp ./agg.sh ./u015_test/
cp ./agg.sh ./u01_test/
cp ./agg.sh ./u035_test/
cd ./u015_test
sh ./agg.sh
cd ../u01_test
sh ./agg.sh
cd ../u035_test
sh ./agg.sh
cd ../
cp ./u015_test/result.csv ./result-015.csv
cp ./u01_test/result.csv ./result-01.csv
cp ./u035_test/result.csv ./result-035.csv
