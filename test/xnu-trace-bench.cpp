#include "xnu-trace/xnu-trace.h"

#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <set>

#include <mach/mach_time.h>

#include <benchmark/benchmark.h>
#include <fmt/format.h>
#define XXH_INLINE_ALL
// #define XXH_NAMESPACE xnu_trace_bench_
#include <xxhash-xnu-trace/xxhash.h>

namespace fs = std::filesystem;

static void BM_lookup_inst(benchmark::State &state) {
    const auto trace    = TraceLog("harness.bundle");
    const auto &regions = trace.macho_regions();
    std::vector<uint64_t> addrs;
    for (const auto &region : regions.regions()) {
        addrs.emplace_back(region.base + 4);
    }
    const auto num_addrs  = addrs.size();
    const auto *addrs_buf = addrs.data();

    size_t i = 0;

    for (auto _ : state) {
        benchmark::DoNotOptimize(regions.lookup_inst(addrs_buf[i % num_addrs]));
        ++i;
    }
}

BENCHMARK(BM_lookup_inst);

static void BM_lookup_inst_from_trace(benchmark::State &state) {
    const auto trace    = TraceLog("harness.bundle");
    const auto &regions = trace.macho_regions();
    std::vector<uint64_t> addrs;
    addrs.resize(trace.num_inst());
    size_t i = 0;
    for (const auto &[tid, log] : trace.parsed_logs()) {
        for (const auto msg : log) {
            addrs[i] = msg.pc;
            ++i;
        }
    }
    const auto num_addrs  = addrs.size();
    const auto *addrs_buf = addrs.data();

    i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(regions.lookup_inst(addrs_buf[i % num_addrs]));
        ++i;
    }
}

BENCHMARK(BM_lookup_inst_from_trace);

static void BM_lookup_inst_mine(benchmark::State &state) {
    const auto trace    = TraceLog("harness.bundle");
    const auto &regions = trace.macho_regions();
    std::vector<uint64_t> addrs;
    for (const auto &region : regions.regions()) {
        addrs.emplace_back(region.base + 4);
    }
    const auto num_addrs  = addrs.size();
    const auto *addrs_buf = addrs.data();

    size_t i = 0;

    for (auto _ : state) {
        benchmark::DoNotOptimize(regions.lookup_inst_mine(addrs_buf[i % num_addrs]));
        ++i;
    }
}

BENCHMARK(BM_lookup_inst_mine);

static void BM_lookup_inst_mine_from_trace(benchmark::State &state) {
    const auto trace    = TraceLog("harness.bundle");
    const auto &regions = trace.macho_regions();
    std::vector<uint64_t> addrs;
    addrs.resize(trace.num_inst());
    size_t i = 0;
    for (const auto &[tid, log] : trace.parsed_logs()) {
        for (const auto msg : log) {
            addrs[i] = msg.pc;
            ++i;
        }
    }
    const auto num_addrs  = addrs.size();
    const auto *addrs_buf = addrs.data();

    i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(regions.lookup_inst_mine(addrs_buf[i % num_addrs]));
        ++i;
    }
}

BENCHMARK(BM_lookup_inst_mine_from_trace);

static void BM_histogram_add(benchmark::State &state) {
    const auto trace    = TraceLog("harness.bundle");
    const auto &regions = trace.macho_regions();
    std::vector<uint32_t> instrs;
    instrs.resize(trace.num_inst());
    size_t i = 0;
    for (const auto &[tid, log] : trace.parsed_logs()) {
        for (const auto msg : log) {
            instrs[i] = regions.lookup_inst(msg.pc);
            ++i;
        }
    }
    const auto num_instrs  = instrs.size();
    const auto *instrs_buf = instrs.data();

    ARM64InstrHistogram hist;
    i = 0;
    for (auto _ : state) {
        hist.add(instrs_buf[i % num_instrs]);
        ++i;
    }
}

// BENCHMARK(BM_histogram_add);

static void BM_xxhash64(benchmark::State &state) {
    uint64_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(XXH64((char *)&i, sizeof(i), i));
        uint64_t i2 = 2 * i;
        benchmark::DoNotOptimize(XXH64((char *)&i2, sizeof(i2), i2));
        ++i;
    }
}

BENCHMARK(BM_xxhash64);

static void BM_xxhash3_64(benchmark::State &state) {
    uint64_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(XXH3_64bits_withSeed((char *)&i, sizeof(i), i));
        ++i;
    }
}

BENCHMARK(BM_xxhash3_64);

static void BM_xnu_commpage_time_seconds(benchmark::State &state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(xnu_commpage_time_seconds());
    }
}

BENCHMARK(BM_xnu_commpage_time_seconds);

static void BM_mach_absolute_time(benchmark::State &state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(mach_absolute_time());
    }
}

BENCHMARK(BM_mach_absolute_time);

static void BM_mach_approximate_time(benchmark::State &state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(mach_approximate_time());
    }
}

BENCHMARK(BM_mach_approximate_time);

BENCHMARK_MAIN();
