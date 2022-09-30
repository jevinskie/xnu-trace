#include "xnu-trace/log_structs.h"
#include "common-internal.h"

void log_arm64_cpu_context::update(const log_msg &msg) {
    if (auto sync_ctx = msg.sync_ctx()) {
        memcpy(this, sync_ctx, sizeof(*this));
        return;
    }
    if (msg.pc_branched()) {
        pc = msg.pc();
    }
    if (msg.sp_changed()) {
        sp = msg.sp();
    }
    auto gpr_ptr = &x[0];
    for (uint32_t i = 0; i < msg.num_gpr(); ++i) {
        gpr_ptr[msg.gpr_idx(i)] = msg.gpr(i);
    }
    for (uint32_t i = 0; i < msg.num_vec(); ++i) {
        v[msg.vec_idx(i)] = msg.vec(i);
    }
}
