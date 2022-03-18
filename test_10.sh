rm -rf ./noctrl_res
mkdir ./noctrl_res
./statesimul.out
mv ./SA_prof_noRR.csv ./noctrl_res/SA_prof_noctrl1.csv
./statesimul.out
mv ./SA_prof_noRR.csv ./noctrl_res/SA_prof_noctrl2.csv
./statesimul.out
mv ./SA_prof_noRR.csv ./noctrl_res/SA_prof_noctrl3.csv
./statesimul.out
mv ./SA_prof_noRR.csv ./noctrl_res/SA_prof_noctrl4.csv
./statesimul.out
mv ./SA_prof_noRR.csv ./noctrl_res/SA_prof_noctrl5.csv
