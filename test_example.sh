### program execution flag explanation
### ./statesimul.out GC_POLICY W_POLICY RR_POLICY PROFILE_GEN TASKNUM TASKUTIL SKEWFLAG T_LOC S_LOC SKEWNUM INITCYC [LAT_MODE]
### LAT_MODE (optional, decision-time only): STATE (default) | FIXED_S | FIXED_E
###   - STATE   : LaWL sees dynamic (state-aware) latency (existing behavior)
###   - FIXED_S : LaWL sees fixed latency using STARTW/STARTR/STARTE (fresh-block)
###   - FIXED_E : LaWL sees fixed latency using ENDW/ENDR/ENDE       (worn-block)
### Ground-truth req->exec, utilization overflow, and MAXPE termination remain state-aware.

### TASK GENERATOR
# ./statesimul.out NO NO NO TASKGEN 4 0.3 -1 -2.0 -2.0 0
### WORKLOAD GENERATOR
# ./statesimul.out NO NO NO WORKGEN 4 0.3 -1 0.05 0.95 0

### SIMULATOR START
### BASIC FLASH OPERATION (EXCEPT wear-leveling)
# ./statesimul.out NO NO SKIPRR nogen 4 0.3 -1 0.05 0.95 0
### MOTIVATION EXPERIMENT (write: only targeting the young block for write | RR: NONE)
# ./statesimul.out NO MOTIVALLY SKIPRR nogen 4 0.3 -1 0.05 0.95 0
### MOTIVATION EXPERIMENT (write: only targeting the young block for write | RR: static wear-leveling)
# ./statesimul.out NO MOTIVALLY BASE005 nogen 4 0.3 -1 0.05 0.95 0
### OUR TECHNIQUE EXPERIMENT (write: clustering | GC : targeting the victim block according to utilization | RR: relocation)
### (a) dynamic latency criterion  -> outputs LaWL_*.csv
./statesimul.out UTILGC INVW RR005 nogen 4 0.3 -1 0.05 0.95 0
### (b) fixed START latency criterion -> outputs LaWL_fixedS_*.csv
# ./statesimul.out UTILGC INVW RR005 nogen 4 0.3 -1 0.05 0.95 0 0 FIXED_S
### (c) fixed END latency criterion   -> outputs LaWL_fixedE_*.csv
# ./statesimul.out UTILGC INVW RR005 nogen 4 0.3 -1 0.05 0.95 0 0 FIXED_E

### WAO-GC (Zhang et al. 2015): greedy least-valid victim + P/E-cycle WL tiebreak,
### GC postponed until only one free block remains (partial-GC interleaving is
### provided naturally by the per-page GC request queue). Termination conditions
### (utilization overflow, MAXPE reach) are unchanged.  Outputs WAOGC_*.csv.
# ./statesimul.out WAOGC NO SKIPRR nogen 4 0.3 -1 0.05 0.95 0