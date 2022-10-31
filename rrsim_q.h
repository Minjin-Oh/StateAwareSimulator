#include "stateaware.h"
long gen_read_rr(int vic1, int vic2, long cur_cp, long rrp,  meta* metadata, IOhead* rrq);
long gen_write_rr(int vic1, int vic2, long cur_cp, long rrp, meta* metadata, IOhead* rrq);
long gen_erase_rr(int vic1, int vic2, long cur_cp, long rrp, meta* metadata, IOhead* rrq);