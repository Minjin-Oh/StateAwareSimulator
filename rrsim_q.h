#include "stateaware.h"
long gen_read_rr(int vic1, int vic2, int cur_cp, int rrp, meta temp, IOhead* rrq);
long gen_write_rr(int vic1, int vic2, int cur_cp, int rrp, meta temp, IOhead* rrq);
long gen_erase_rr(int vic1, int vic2, int cur_cp, int rrp, meta temp, IOhead* rrq);