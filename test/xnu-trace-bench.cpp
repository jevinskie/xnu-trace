#include "xnu-trace/xnu-trace.h"

#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <set>
#include <type_traits>
#include <utility>

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
            // FIXME
            // addrs[i] = msg.pc;
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

static void BM_histogram_add(benchmark::State &state) {
    const auto trace    = TraceLog("harness.bundle");
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
    for (auto _ : state) {
        hist.add(instrs_buf[i % num_instrs]);
        ++i;
    }
}

// BENCHMARK(BM_histogram_add);

static void BM_xxhash64(benchmark::State &state) {
    uint64_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(xxhash_64::hash(i, i * 7));
    }
}

BENCHMARK(BM_xxhash64);

static void BM_xxhash3_64(benchmark::State &state) {
    uint64_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(xxhash3_64::hash(i, i * 7));
        ++i;
    }
}

BENCHMARK(BM_xxhash3_64);

static void BM_jevhash_64(benchmark::State &state) {
    uint64_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(jevhash_64::hash(i, i * 7));
        ++i;
    }
}

BENCHMARK(BM_jevhash_64);

static void BM_jevhash_32(benchmark::State &state) {
    uint64_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(jevhash_32::hash(i, i * 7));
        ++i;
    }
}

BENCHMARK(BM_jevhash_32);

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

static uint64_t hash_n(uint8_t nbits, uint64_t val) {
    return xxhash3_64::hash(val) & xnutrace::BitVector::bit_mask<uint64_t>(0, nbits);
}

static void BM_NonAtomicBitVectorImpl(benchmark::State &state) {
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint32_t);
    auto bv                 = NonAtomicBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(i, nbits));
    }

    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(bv.get(i % sz));
        ++i;
    }
}

BENCHMARK(BM_NonAtomicBitVectorImpl);

static void BM_NonAtomicBitVector_no_hoist(benchmark::State &state) {
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint32_t);
    auto bv                 = BitVectorFactory<>(nbits, sz);
    for (size_t i = 0; i < sz; ++i) {
        bv->set(i, hash_n(i, nbits));
    }

    const auto cbv = std::add_const_t<decltype(bv.get())>(bv.get());
    size_t i       = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(cbv->get(i % sz)); // loads fptr from vtable
        ++i;
    }
}

BENCHMARK(BM_NonAtomicBitVector_no_hoist);

static void BM_NonAtomicBitVector(benchmark::State &state) {
    constexpr uint8_t nbits = 31;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint32_t);
    auto bv                 = BitVectorFactory<>(nbits, sz);
    for (size_t i = 0; i < sz; ++i) {
        bv->set(i, hash_n(i, nbits));
    }

    const auto cbv = &std::as_const(*bv.get());
    size_t i       = 0;
    benchmark::DoNotOptimize(cbv->get(
        0)); // this dummy call allows the fptr load from vtable to be hoisted out of the loop
    for (auto _ : state) {
        benchmark::DoNotOptimize(cbv->get(i % sz)); // doesn't load fptr from vtable
        ++i;
    }
}

BENCHMARK(BM_NonAtomicBitVector);

static void BM_NonAtomicBitVectorImpl_33(benchmark::State &state) {
    constexpr uint8_t nbits = 33;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint64_t);
    auto bv                 = NonAtomicBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(i, nbits));
    }

    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(bv.get(i % sz));
        ++i;
    }
}

BENCHMARK(BM_NonAtomicBitVectorImpl_33);

static void BM_NonAtomicSplitBitVectorImpl_33(benchmark::State &state) {
    constexpr uint8_t nbits = 33;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint64_t);
    auto bv                 = NonAtomicSplitBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(i, nbits));
    }

    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(bv.get(i % sz));
        ++i;
    }
}

BENCHMARK(BM_NonAtomicSplitBitVectorImpl_33);

static void BM_NonAtomicBitVectorImpl_47(benchmark::State &state) {
    constexpr uint8_t nbits = 47;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint64_t);
    auto bv                 = NonAtomicBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(i, nbits));
    }

    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(bv.get(i % sz));
        ++i;
    }
}

BENCHMARK(BM_NonAtomicBitVectorImpl_47);

static void BM_NonAtomicSplitBitVectorImpl_47(benchmark::State &state) {
    constexpr uint8_t nbits = 47;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint64_t);
    auto bv                 = NonAtomicSplitBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(i, nbits));
    }

    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(bv.get(i % sz));
        ++i;
    }
}

BENCHMARK(BM_NonAtomicSplitBitVectorImpl_47);

static void BM_NonAtomicBitVectorImpl_63(benchmark::State &state) {
    constexpr uint8_t nbits = 63;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint64_t);
    auto bv                 = NonAtomicBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(i, nbits));
    }

    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(bv.get(i % sz));
        ++i;
    }
}

BENCHMARK(BM_NonAtomicBitVectorImpl_63);

static void BM_NonAtomicSplitBitVectorImpl_63(benchmark::State &state) {
    constexpr uint8_t nbits = 63;
    constexpr size_t sz     = 128 * 1024 * 1024 / sizeof(uint64_t);
    auto bv                 = NonAtomicSplitBitVectorImpl<nbits>(sz);
    for (size_t i = 0; i < sz; ++i) {
        bv.set(i, hash_n(i, nbits));
    }

    size_t i = 0;
    for (auto _ : state) {
        benchmark::DoNotOptimize(bv.get(i % sz));
        ++i;
    }
}

BENCHMARK(BM_NonAtomicSplitBitVectorImpl_63);

BENCHMARK_MAIN();
