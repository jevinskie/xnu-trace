#include "common.h"

#include <arch-arm64/arm64dis.h>

void ARM64InstrHistogram::add(uint32_t instr) {
    if (!m_instr_to_op.contains(instr)) {
        Instruction inst_repr;
        assert(aarch64_decompose(instr, &inst_repr, 0) == DECODE_STATUS_OK);
        m_instr_to_op.emplace(instr, (uint16_t)inst_repr.operation);
    }
    const auto op = m_instr_to_op[instr];
    m_op_count.insert_or_assign(op, m_op_count[op] + 1);
    ++m_num_inst;
}

void ARM64InstrHistogram::add_mask(uint32_t instr) {
    if (!m_instr_to_op.contains(instr >> 10)) {
        Instruction inst_repr;
        assert(aarch64_decompose(instr, &inst_repr, 0) == DECODE_STATUS_OK);
        m_instr_to_op.emplace(instr >> 10, (uint16_t)inst_repr.operation);
    }
    const auto op = m_instr_to_op[instr >> 10];
    m_op_count.insert_or_assign(op, m_op_count[op] + 1);
    ++m_num_inst;
}

void ARM64InstrHistogram::add_hash(uint32_t instr) {
    if (!m_instr_to_op.contains(instr)) {
        Instruction inst_repr;
        assert(aarch64_decompose(instr, &inst_repr, 0) == DECODE_STATUS_OK);
        m_instr_to_op.emplace(instr, (uint16_t)inst_repr.operation);
    }
    const auto op = m_instr_to_op[instr];
    m_op_count.insert_or_assign(op, m_op_count[op] + 1);
    ++m_num_inst;
}

void ARM64InstrHistogram::add_lut(uint32_t instr) {
    if (!m_instr_to_op.contains(instr)) {
        Instruction inst_repr;
        assert(aarch64_decompose(instr, &inst_repr, 0) == DECODE_STATUS_OK);
        m_instr_to_op.emplace(instr, (uint16_t)inst_repr.operation);
    }
    const auto op = m_instr_to_op[instr];
    m_op_count.insert_or_assign(op, m_op_count[op] + 1);
    ++m_num_inst;
}

void ARM64InstrHistogram::print(int max_num, unsigned int width) const {
    if (max_num < 0) {
        max_num = (int)m_op_count.size();
    }
    std::vector<std::pair<uint16_t, uint64_t>> sorted;
    for (const auto &it : m_op_count) {
        sorted.emplace_back(it);
    }
    std::sort(
        sorted.begin(), sorted.end(),
        [](const auto &a, const auto &b) -> auto{ return a.second > b.second; });
    sorted.resize(max_num);
    unsigned int idx     = 1;
    const auto max_count = sorted[0].second;
    for (const auto &[op, num] : sorted) {
        Instruction inst{.operation = (Operation)op};
        fmt::print("{:s}\n", fmt::format(std::locale("en_US.UTF-8"), "{:5Ld}: {:8s} {:12Ld} {:s}",
                                         idx, get_operation(&inst), num,
                                         block_str((double)num / max_count, width)));
        ++idx;
    }
}
