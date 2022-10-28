
sed -n '5p' taskparam-rs1.csv > result-1.csv
sed -n '5p' taskparam-rs2.csv > result-2.csv
sed -n '5p' taskparam-rs3.csv > result-3.csv
sed -n '5p' taskparam-rs4.csv > result-4.csv

cat result-*.csv > agg.csv

