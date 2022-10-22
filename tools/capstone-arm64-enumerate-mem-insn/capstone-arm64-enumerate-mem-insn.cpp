#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <set>
#include <string>

#include <arch-arm64/arm64dis.h>
#include <fmt/format.h>
#include <frida-gum.h>

int main() {
    csh cs_handle;
    assert(cs_open(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, &cs_handle) == CS_ERR_OK);
    cs_option(cs_handle, CS_OPT_DETAIL, CS_OPT_ON);
    std::set<std::string> mem_opc;
    for (uint32_t shifted_instr = 0; shifted_instr <= 1 << 22; ++shifted_instr) {
        uint32_t instr = shifted_instr << 10;
        Instruction inst_repr;
        if (aarch64_decompose(instr, &inst_repr, 0) != DECODE_STATUS_OK) {
            continue;
        }
        cs_insn *cs_instr;
        const auto count =
            cs_disasm(cs_handle, (const uint8_t *)&instr, sizeof(instr), 0, 0, &cs_instr);
        assert(count == 0 || count == 1);
        if (count == 0) {
            // diff between binary ninja and capstone
            cs_free(cs_instr, count);
            continue;
        }
        bool is_mem     = false;
        cs_arm64 *arm64 = &cs_instr->detail->arm64;
        for (int i = 0; i < arm64->op_count; ++i) {
            cs_arm64_op *op = &arm64->operands[i];
            if (op->type == ARM64_OP_MEM) {
                is_mem = true;
            }
        }
        cs_free(cs_instr, count);
        if (is_mem) {
            mem_opc.emplace(get_operation(&inst_repr));
        }
    }
    for (const auto &opc : mem_opc) {
        fmt::print("opc: {:s}\n", opc);
    }
    return 0;
}
