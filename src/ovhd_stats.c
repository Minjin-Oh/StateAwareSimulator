#include "ovhd_stats.h"
#include <stdlib.h>
#include <string.h>

/* Histogram: 1-us linear bins for [0, 1024) us  -> exact percentiles in
 * the region where decisions actually live (tens of us), plus power-of-two
 * tail buckets for [1024 us, ~2 s) so rare spikes are still captured. */
#define OVHD_LIN_BINS 1024
#define OVHD_LOG_BINS 12   /* 1024..2047, 2048..4095, ... ~2^21 us, +overflow */
#define OVHD_BINS (OVHD_LIN_BINS + OVHD_LOG_BINS)

typedef struct {
    long   cnt;
    double sum;
    long   min_v, max_v;
    long   max_at_sim;          /* simulated time of the max sample */
    long   first_sim, last_sim; /* simulated-time span covered */
    long   hist[OVHD_BINS];
} ovhd_stat_t;

static const char* g_names[OVHD_NCAT] =
    { "write_decision", "gc_decision", "rr_decision",
      "clustering_update", "sim_workload_io",
      "write_decision_incl_cluster" };

/* time stashed for exclusion at the enclosing write-decision site:
 * clustering computation + simulator-oracle trace reads */
static long g_pending_excl = 0;
static int  g_pending_had_cluster = 0;

static ovhd_stat_t g_st[OVHD_NCAT];
static FILE* g_raw = NULL;
static char  g_base[256] = "ovhd_dist";
static int   g_inited = 0, g_dumped = 0;
static double g_tick_usec = 1.0;

static int bin_of(long v){
    if(v < 0) v = 0;
    if(v < OVHD_LIN_BINS) return (int)v;
    long x = v >> 10;                 /* v / 1024 */
    int b = 0;
    while(x > 1 && b < OVHD_LOG_BINS - 1){ x >>= 1; b++; }
    return OVHD_LIN_BINS + b;
}
static long bin_lo(int b){
    if(b < OVHD_LIN_BINS) return (long)b;
    return 1024L << (b - OVHD_LIN_BINS);
}

void ovhd_set_sim_tick_usec(double t){ if(t > 0) g_tick_usec = t; }

void ovhd_init(const char* basename){
    if(g_inited) return;
    g_inited = 1;
    if(basename && basename[0]){
        strncpy(g_base, basename, sizeof(g_base)-1);
        g_base[sizeof(g_base)-1] = '\0';
    }
    for(int i = 0; i < OVHD_NCAT; i++){
        memset(&g_st[i], 0, sizeof(ovhd_stat_t));
        g_st[i].min_v = -1;
        g_st[i].first_sim = -1;
    }
#ifndef OVHD_NO_RAW
    {
        char fn[300];
        snprintf(fn, sizeof(fn), "%s_raw.csv", g_base);
        g_raw = fopen(fn, "w");
        if(g_raw) fprintf(g_raw, "category,sim_time,usec\n");
    }
#endif
    atexit(ovhd_dump_all);
}

void ovhd_record(ovhd_cat_t cat, long usec, long sim_time){
    if(!g_inited) ovhd_init(NULL);
    if(cat < 0 || cat >= OVHD_NCAT) return;
    ovhd_stat_t* s = &g_st[cat];
    s->cnt++;
    s->sum += (double)usec;
    if(s->min_v < 0 || usec < s->min_v) s->min_v = usec;
    if(usec > s->max_v){ s->max_v = usec; s->max_at_sim = sim_time; }
    if(s->first_sim < 0) s->first_sim = sim_time;
    s->last_sim = sim_time;
    s->hist[bin_of(usec)]++;
    if(cat == OVHD_CLUSTER){ g_pending_excl += usec; g_pending_had_cluster = 1; }
    if(cat == OVHD_WSIM)   { g_pending_excl += usec; }
#ifndef OVHD_NO_RAW
    if(g_raw) fprintf(g_raw, "%s,%ld,%ld\n", g_names[cat], sim_time, usec);
#endif
}

long ovhd_take_pending_excl(int* had_cluster){
    long v = g_pending_excl;
    if(had_cluster) *had_cluster = g_pending_had_cluster;
    g_pending_excl = 0;
    g_pending_had_cluster = 0;
    return v;
}

static long pctl(const ovhd_stat_t* s, double p){
    if(s->cnt == 0) return 0;
    long target = (long)(p * (double)s->cnt);
    if(target < 1) target = 1;
    long acc = 0;
    for(int b = 0; b < OVHD_BINS; b++){
        acc += s->hist[b];
        if(acc >= target) return bin_lo(b);
    }
    return s->max_v;
}

void ovhd_dump_all(void){
    if(!g_inited || g_dumped) return;
    g_dumped = 1;

    char fn[300];
    snprintf(fn, sizeof(fn), "%s_summary.csv", g_base);
    FILE* f = fopen(fn, "w");

    /* simulated wall-span (seconds) for rate & aggregate-demand math:
     * use the widest first..last sim-time span seen across categories. */
    long lo = -1, hi = 0;
    for(int i = 0; i < OVHD_NCAT; i++){
        if(g_st[i].cnt == 0) continue;
        if(lo < 0 || g_st[i].first_sim < lo) lo = g_st[i].first_sim;
        if(g_st[i].last_sim > hi) hi = g_st[i].last_sim;
    }
    double sim_sec = (lo >= 0 && hi > lo)
                   ? ((double)(hi - lo) * g_tick_usec) / 1e6 : 0.0;

    const char* hdr = "category,count,mean_us,min_us,p50_us,p90_us,p95_us,"
                      "p99_us,p999_us,max_us,max_at_sim,"
                      "rate_per_sim_sec,demand_us_per_sim_sec_mean,"
                      "demand_us_per_sim_sec_max\n";
    if(f) fputs(hdr, f);
    fprintf(stderr, "\n==== decision-overhead distributions ====\n%s", hdr);

    double u_mean_total = 0.0, u_max_total = 0.0; /* controller cats only */
    for(int i = 0; i < OVHD_NCAT; i++){
        ovhd_stat_t* s = &g_st[i];
        double mean = s->cnt ? s->sum / (double)s->cnt : 0.0;
        double rate = (sim_sec > 0) ? (double)s->cnt / sim_sec : 0.0;
        double d_mean = rate * mean;
        double d_max  = rate * (double)s->max_v;
        char line[512];
        snprintf(line, sizeof(line),
            "%s,%ld,%.2f,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%.4f,%.2f,%.2f\n",
            g_names[i], s->cnt, mean, (s->min_v < 0 ? 0 : s->min_v),
            pctl(s,0.50), pctl(s,0.90), pctl(s,0.95),
            pctl(s,0.99), pctl(s,0.999), s->max_v, s->max_at_sim,
            rate, d_mean, d_max);
        if(f) fputs(line, f);
        fputs(line, stderr);
        if(i == OVHD_WRITE || i == OVHD_GC || i == OVHD_RR ||
           i == OVHD_CLUSTER){
            u_mean_total += d_mean;
            u_max_total  += d_max;
        }
    }
    /* aggregate controller demand: rate x cost summed over decision
     * categories. Clustering is included as its own periodic term
     * (rate = boundary-update rate, not the write rate). Excluded:
     * WSIM (simulator infrastructure) and WRITE_CL (diagnostic view;
     * its cost is already counted via OVHD_WRITE + OVHD_CLUSTER).
     * us of CPU per simulated second /1e6 = util. */
    fprintf(stderr,
        "---- aggregate controller demand (write+gc+rr+cluster) ----\n"
        "sim span: %.1f s | mean-cost basis: %.2f us/s (util %.6f%%) | "
        "max-cost basis: %.2f us/s (util %.6f%%)\n"
        "(sim_workload_io & write_decision_incl_cluster reported "
        "separately; clustering excludes oracle trace reads)\n",
        sim_sec, u_mean_total, u_mean_total / 1e4,
        u_max_total, u_max_total / 1e4);
    if(f){
        fprintf(f, "#aggregate_write_gc_rr_cluster,sim_sec=%.1f,"
                   "util_mean_pct=%.6f,util_maxcost_pct=%.6f\n",
                sim_sec, u_mean_total / 1e4, u_max_total / 1e4);
        fclose(f);
    }
    if(g_raw){ fclose(g_raw); g_raw = NULL; }
}
