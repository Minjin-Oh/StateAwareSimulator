mkdir ./0.15-res
mkdir ./0.3-res
cp ./0.15/lifetime.csv ./0.15-res/lifetime-1.csv
cp ./0.3/lifetime.csv ./0.3-res/lifetime-1.csv
for var2 in $(seq 2 5)
do
                cp ./0.15_$var2/lifetime.csv ./0.15-res/lifetime-$var2.csv
                cp ./0.3_$var2/lifetime.csv ./0.3-res/lifetime-$var2.csv
done

