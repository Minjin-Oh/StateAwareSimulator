### program execution flag explanation
### ./statesimul.out GC_POLICY W_POLICY RR_POLICY PROFILE_GEN TASKNUM TASKUTIL SKEWFLAG T_LOC S_LOC SKEWNUM INITCYC

### TASK GENERATOR
# ./statesimul.out NO NO NO TASKGEN 4 0.3 -1 -2.0 -2.0 0
# ./statesimul.out NO NO NO TASKGEN 4 0.25 -1 -2.0 -2.0 0
# ./statesimul.out NO NO NO TASKGEN 4 0.2 -1 -2.0 -2.0 0
# ./statesimul.out NO NO NO TASKGEN 4 0.15 -1 -2.0 -2.0 0
# ./statesimul.out NO NO NO TASKGEN 4 0.1 -1 -2.0 -2.0 0

### WORKLOAD GENERATOR
# ./statesimul.out NO NO NO WORKGEN 4 0.3 -1 0.05 0.95 0

# 공통 결과 파일 이름 패턴 (필요시 수정)
CSV_PATTERN_baseline="no*.csv"
CSV_PATTERN_WLcomb="WLcomb*.csv"
CSV_PATTERN_wonly="wonly*.csv"
CSV_PATTERN_wgc="wgc*.csv"
CSV_PATTERN_ours="ours*.csv"

# 결과 저장 디렉토리 베이스
RESULT_DIR="./results"

# 디렉토리 없으면 생성
mkdir -p "$RESULT_DIR"

### SIMULATOR START
# 1. BASELINE
echo "Running: BASIC FLASH OPERATION"
./statesimul.out NO NO SKIPRR nogen 4 0.3 -1 0.05 0.95 0
mkdir -p "$RESULT_DIR/baseline"
mv $CSV_PATTERN_baseline "$RESULT_DIR/baseline/"

# 2. MOTIVATION EXPERIMENT
echo "Running: MOTIVATION EXPERIMENT"
./statesimul.out NO MOTIVALLY BASE005 nogen 4 0.3 -1 0.05 0.95 0
mkdir -p "$RESULT_DIR/WL"
mv $CSV_PATTERN_WLcomb "$RESULT_DIR/WL/"

# 3. OUR TECHNIQUE
## 3-1. write only
echo "Running: OUR TECHNIQUE - write only"
./statesimul.out NO INVW SKIPRR nogen 4 0.3 -1 0.05 0.95 0
mkdir -p "$RESULT_DIR/write_only"
mv $CSV_PATTERN_wonly "$RESULT_DIR/write_only/"

## 3-2. write + GC
echo "Running: OUR TECHNIQUE - write + GC"
./statesimul.out UTILGC INVW SKIPRR nogen 4 0.3 -1 0.05 0.95 0
mkdir -p "$RESULT_DIR/write_gc"
mv $CSV_PATTERN_wgc "$RESULT_DIR/write_gc/"

## 3-3. write + GC + relocation
echo "Running: OUR TECHNIQUE - write + GC + relocation"
./statesimul.out UTILGC INVW RR005 nogen 4 0.3 -1 0.05 0.95 0
mkdir -p "$RESULT_DIR/write_gc_rr"
mv $CSV_PATTERN_ours "$RESULT_DIR/write_gc_rr/"