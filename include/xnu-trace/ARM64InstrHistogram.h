#pragma once

#include "common.h"

class XNUTRACE_EXPORT ARM64InstrHistogram {
public:
    ARM64InstrHistogram();
    XNUTRACE_INLINE void add(uint32_t instr);

    void print(int max_num = 64, unsigned int width = 80) const;

private:
    std::vector<uint16_t> m_instr_to_op_lut;
    std::vector<uint64_t> m_op_count_lut;
};
