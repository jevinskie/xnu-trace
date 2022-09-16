#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <set>

#include <benchmark/benchmark.h>
#include <fmt/format.h>

namespace fs = std::filesystem;

static void BM_lookup_inst(benchmark::State &state) {
    const auto trace   = TraceLog("harness.bundle");
    const auto regions = trace.macho_regions();
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

BENCHMARK_MAIN();
