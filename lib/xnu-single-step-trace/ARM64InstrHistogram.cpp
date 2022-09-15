#include "common.h"

#include <arch-arm64/arm64dis.h>

void ARM64InstrHistogram::add(uint32_t instr) {
    if (!m_instr_to_op.contains(instr)) {
        Instruction inst_repr;
        assert(aarch64_decompose(instr, &inst_repr, 0) == DECODE_STATUS_OK);
        m_instr_to_op.emplace(std::make_pair(instr, (uint16_t)inst_repr.operation));
    }
    const auto op = m_instr_to_op[instr];
    m_op_count.insert_or_assign(op, m_op_count[op] + 1);
    ++m_num_inst;
}

void ARM64InstrHistogram::print(int max_num, unsigned int width) const {
    if (max_num < 0) {
        max_num = m_op_count.size();
    }
    std::vector<std::pair<uint16_t, uint64_t>> sorted;
    for (const auto &it : m_op_count) {
        sorted.emplace_back(it);
    }
    std::sort(
        sorted.begin(), sorted.end(),
        [](const auto &a, const auto &b) -> auto{ return a.second >= b.second; });
    sorted.resize(max_num);
    unsigned int idx = 0;
    for (const auto &[op, num] : sorted) {
        Instruction inst{.operation = (Operation)op};
        fmt::print("{:4d}: {:s} {:d} {:s}\n", idx, get_operation(&inst), num,
                   block_str((double)num / m_num_inst, width));
        ++idx;
    }
}
