
sed -n '5p' taskparam-ws1.csv > result-1.csv
sed -n '5p' taskparam-ws2.csv > result-2.csv
sed -n '5p' taskparam-ws3.csv > result-3.csv
sed -n '5p' taskparam-ws4.csv > result-4.csv

cat result-*.csv > agg.csv

