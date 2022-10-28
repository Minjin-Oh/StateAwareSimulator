
sed -n '5p' taskparam-5050.csv > result-5050.csv
sed -n '5p' taskparam-6040.csv > result-6040.csv
sed -n '5p' taskparam-7030.csv > result-7030.csv
sed -n '5p' taskparam-8020.csv > result-8020.csv
sed -n '5p' taskparam-9010.csv > result-9010.csv
sed -n '5p' taskparam-9505.csv > result-9505.csv

cat result-*.csv > agg.csv

