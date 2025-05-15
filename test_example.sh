### program execution flag explanation
### ./statesimul.out GC_POLICY W_POLICY RR_POLICY PROFILE_GEN TASKNUM TASKUTIL SKEWFLAG T_LOC S_LOC SKEWNUM INITCYC

### TASK GENERATOR
# ./statesimul.out NO NO NO TASKGEN 4 0.3 -1 -2.0 -2.0 0
### WORKLOAD GENERATOR
# ./statesimul.out NO NO NO WORKGEN 4 0.3 -1 0.05 0.95 0

### SIMULATOR START
### BASIC FLASH OPERATION (EXCEPT wear-leveling)
# ./statesimul.out NO NO SKIPRR nogen 4 0.3 -1 0.05 0.95 0
### MOTIVATION EXPERIMENT (write: only targeting the young block for write | RR: dynamic & static wear-leveling)
# ./statesimul.out NO MOTIVALLY BASE005 nogen 4 0.3 -1 0.05 0.95 0
### OUR TECHNIQUE EXPERIMENT (write: clustering | GC : targeting the victim block according to utilization | RR: relocation)
./statesimul.out UTILGC INVW RR005 nogen 4 0.3 -1 0.05 0.95 0