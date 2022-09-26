#include "xnu-trace/xnu-trace.h"

#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <set>

#include <mach/mach_time.h>

#define ANKERL_NANOBENCH_IMPLEMENT
#include <fmt/format.h>
#include <nanobench.h>
#define XXH_INLINE_ALL
#include <xxhash-xnu-trace/xxhash.h>

namespace fs = std::filesystem;
using namespace ankerl;

static void BM_lookup_inst(const TraceLog &trace) {
    const auto &regions = trace.macho_regions();
    std::vector<uint64_t> addrs;
    for (const auto &region : regions.regions()) {
        addrs.emplace_back(region.base + 4);
    }
    const auto num_addrs  = addrs.size();
    const auto *addrs_buf = addrs.data();

    size_t i = 0;

    nanobench::Bench().run("MachORegions::lookup_inst even dist", [&]() {
        nanobench::doNotOptimizeAway(regions.lookup_inst(addrs_buf[i % num_addrs]));
        ++i;
    });
}

static void BM_lookup_inst_from_trace(const TraceLog &trace) {
    const auto &regions = trace.macho_regions();
    std::vector<uint64_t> addrs;
    addrs.resize(trace.num_inst());
    size_t i = 0;
    for (const auto &[tid, log] : trace.parsed_logs()) {
        for (const auto msg : log) {
            // FIXME
            // addrs[i] = msg.pc;
            ++i;
        }
    }
    const auto num_addrs  = addrs.size();
    const auto *addrs_buf = addrs.data();

    i = 0;
    nanobench::Bench().run("MachORegions::lookup_inst trace dist", [&]() {
        nanobench::doNotOptimizeAway(regions.lookup_inst(addrs_buf[i % num_addrs]));
        ++i;
    });
}

static void BM_histogram_add(const TraceLog &trace) {
    const auto &regions = trace.macho_regions();
    std::vector<uint32_t> instrs;
    instrs.resize(trace.num_inst());
    size_t i = 0;
    for (const auto &[tid, log] : trace.parsed_logs()) {
        for (const auto msg : log) {
            // FIXME
            // instrs[i] = regions.lookup_inst(msg.pc);
            ++i;
        }
    }
    const auto num_instrs  = instrs.size();
    const auto *instrs_buf = instrs.data();

    ARM64InstrHistogram hist;
    i = 0;
    nanobench::Bench().run("ARM64InstrHistogram::add", [&]() {
        hist.add(instrs_buf[i % num_instrs]);
        ++i;
    });
}

static void BM_xxhash64() {
    uint64_t i = 0;
    nanobench::Bench().run("XXH64", [&]() {
        nanobench::doNotOptimizeAway(XXH64((char *)&i, sizeof(i), i));
        ++i;
    });
}

static void BM_xxhash3_64() {
    uint64_t i = 0;
    nanobench::Bench().run("XXH3_64bits_withSeed", [&]() {
        nanobench::doNotOptimizeAway(XXH3_64bits_withSeed((char *)&i, sizeof(i), i));
        ++i;
    });
}

static void BM_xnu_commpage_time_seconds() {
    nanobench::Bench().run("xnu_commpage_time_seconds", [&]() {
        nanobench::doNotOptimizeAway(xnu_commpage_time_seconds());
    });
}

static void BM_mach_absolute_time() {
    nanobench::Bench().run("mach_absolute_time", [&]() {
        nanobench::doNotOptimizeAway(mach_absolute_time());
    });
}

static void BM_mach_approximate_time() {
    nanobench::Bench().run("mach_approximate_time", [&]() {
        nanobench::doNotOptimizeAway(mach_approximate_time());
    });
}

int main(void) {
    const auto trace = TraceLog("harness.bundle");

    BM_lookup_inst(trace);
    BM_lookup_inst_from_trace(trace);
    BM_histogram_add(trace);
    BM_xxhash64();
    BM_xxhash3_64();
    BM_xnu_commpage_time_seconds();
    BM_mach_absolute_time();
    BM_mach_approximate_time();

    return 0;
}
