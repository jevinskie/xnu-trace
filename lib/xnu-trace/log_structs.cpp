#include "xnu-trace/log_structs.h"
#include "common-internal.h"

void log_arm64_cpu_context::update(const log_msg &msg) {
    if (msg.pc_branched()) {
        pc = msg.pc();
    }
    if (msg.sp_changed()) {
        sp = msg.sp();
    }
    auto gpr_ptr = &x[0];
    for (const auto &[gpr_idx, gpr_val] : msg.changed_gpr()) {
        gpr_ptr[gpr_idx] = gpr_val;
    }
    for (const auto &[vec_idx, vec_val] : msg.changed_vec()) {
        v[vec_idx] = vec_val;
    }
}
