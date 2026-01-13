### program execution flag explanation
### ./statesimul.out GC_POLICY W_POLICY RR_POLICY PROFILE_GEN TASKNUM TASKUTIL SKEWFLAG T_LOC S_LOC SKEWNUM INITCYC

### TASK GENERATOR
./statesimul.out NO NO NO TASKGEN 4 0.3 -1 0.05 0.95 0
# ./statesimul.out NO NO NO TASKGEN 4 0.25 -1 0.05 0.95 0
# ./statesimul.out NO NO NO TASKGEN 4 0.2 -1 0.05 0.95 0
# ./statesimul.out NO NO NO TASKGEN 4 0.15 -1 0.05 0.95 0
# ./statesimul.out NO NO NO TASKGEN 4 0.1 -1 0.05 0.95 0

### WORKLOAD GENERATOR
./statesimul.out NO NO NO WORKGEN 4 0.3 -1 0.05 0.95 0

### SIMULATOR START
# 1. BASELINE (EXCEPT wear-leveling)
./statesimul.out NO NO SKIPRR nogen 4 0.3 -1 0.05 0.95 0

# 2. MOTIVATION EXPERIMENT (write: only targeting the young block for write | RR: dynamic & static wear-leveling)
./statesimul.out NO MOTIVALLY BASE005 nogen 4 0.3 -1 0.05 0.95 0

# 3. OUR TECHNIQUE (write: clustering | GC : targeting the victim block according to utilization | RR: relocation)
## 3-1. write only
./statesimul.out NO INVW SKIPRR nogen 4 0.3 -1 0.05 0.95 0

## 3-2. write + GC
./statesimul.out UTILGC INVW SKIPRR nogen 4 0.3 -1 0.05 0.95 0

## 3-3. write + GC + relocation
./statesimul.out UTILGC INVW RR005 nogen 4 0.3 -1 0.05 0.95 0