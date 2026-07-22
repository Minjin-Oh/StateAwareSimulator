#ifndef OVHD_STATS_H
#define OVHD_STATS_H

/* NOTE: _GNU_SOURCE must be defined before any system header for
 * CLOCK_MONOTONIC_RAW to be exposed. Safest is to add -D_GNU_SOURCE
 * to CFLAGS; the guard below only helps if this header is included
 * before <time.h> is first pulled in. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <time.h>

typedef enum {
    OVHD_WRITE,      /* write-block selection, pure (clustering excluded) */
    OVHD_GC,         /* GC-pair selection */
    OVHD_RR,         /* relocation (LaWL-S) admission/selection */
    OVHD_CLUSTER,    /* cluster-boundary update (oracle trace reads excluded) */
    OVHD_WSIM,       /* simulator workload-file reads (infrastructure) */
    OVHD_WRITE_CL,   /* diagnostic: raw write decisions that included a
                        clustering update (= pure write + clustering) */
    OVHD_NCAT
} ovhd_cat_t;

void ovhd_init(const char* basename);
void ovhd_record(ovhd_cat_t cat, long usec, long sim_time);
void ovhd_dump_all(void);
void ovhd_set_sim_tick_usec(double t);

/* Returns and clears the accumulated time to exclude from the enclosing
 * write-decision measurement: clustering-update computation and
 * simulator-oracle trace reads (OVHD_CLUSTER / OVHD_WSIM records made
 * since the last call). If had_cluster is non-NULL, it is set to 1 when
 * a clustering update occurred in the interval, 0 otherwise. */
long ovhd_take_pending_excl(int* had_cluster);

/* Monotonic microsecond timestamp for overhead measurement.
 * CLOCK_MONOTONIC_RAW is immune to NTP slewing/stepping; falls back
 * to CLOCK_MONOTONIC where unavailable. Link with -lrt on glibc<2.17. */
static inline long ovhd_now_us(void){
    struct timespec ts;
#ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
}

#endif /* OVHD_STATS_H */
