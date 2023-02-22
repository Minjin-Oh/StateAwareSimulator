for var in $(seq 10)
do
 sh ./gen_tsk.sh
 sh ./test_1set.sh
done
