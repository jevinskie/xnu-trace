#include "common.h"

#include <arch-arm64/arm64dis.h>

ARM64InstrHistogram::ARM64InstrHistogram() {
    m_instr_to_op_lut.resize(1 << 22, UINT16_MAX);
    m_op_count_lut.resize(UINT16_MAX);
}

void ARM64InstrHistogram::add(uint32_t instr) {
    if (m_instr_to_op_lut[instr >> 10] == UINT16_MAX) {
        Instruction inst_repr;
        assert(aarch64_decompose(instr, &inst_repr, 0) == DECODE_STATUS_OK);
        m_instr_to_op_lut[instr >> 10] = (uint16_t)inst_repr.operation;
    }
    const auto op = m_instr_to_op_lut[instr >> 10];
    ++m_op_count_lut[op];
    ++m_num_inst;
}

void ARM64InstrHistogram::print(int max_num, unsigned int width) const {
    std::map<uint16_t, uint64_t> op_counts;
    size_t i = 0;
    for (const auto op_count : m_op_count_lut) {
        if (op_count) {
            op_counts.emplace(i, op_count);
        }
        ++i;
    }
    if (max_num < 0) {
        max_num = (int)op_counts.size();
    }
    std::vector<std::pair<uint16_t, uint64_t>> sorted;
    for (const auto &it : op_counts) {
        sorted.emplace_back(it);
    }
    std::sort(
        sorted.begin(), sorted.end(),
        [](const auto &a, const auto &b) -> auto{ return a.second > b.second; });
    sorted.resize(max_num);
    const auto max_count = sorted[0].second;
    i                    = 1;
    for (const auto &[op, num] : sorted) {
        Instruction inst{.operation = (Operation)op};
        fmt::print("{:s}\n", fmt::format(std::locale("en_US.UTF-8"), "{:5Ld}: {:8s} {:12Ld} {:s}",
                                         i, get_operation(&inst), num,
                                         block_str((double)num / max_count, width)));
        ++i;
    }
}
