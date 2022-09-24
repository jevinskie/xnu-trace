#include "xnu-trace/xnu-trace.h"

#undef NDEBUG
#include <cassert>
#include <vector>

#include <fmt/chrono.h>
#include <fmt/format.h>

int main(int argc, const char **argv) {
    if (argc != 2) {
        fmt::print("usage: {:s} <keys.bin>\n");
        return -1;
    }

    Signpost("mph", "read file start", true,
             fmt::format("time: {:%H:%M:%S}", fmt::localtime(std::time(nullptr))));

    auto load_sp = Signpost("mph", "loading");
    load_sp.start();
    const auto buf     = read_file(argv[1]);
    const auto raw_buf = (uint64_t *)buf.data();
    const auto nkeys   = buf.size() / sizeof(uint64_t);
    std::vector<uint64_t> keys;
    keys.reserve(nkeys);
    for (size_t i = 0; i < nkeys; ++i) {
        keys.emplace_back(raw_buf[i]);
    }
    load_sp.end([&](uint64_t ns) {
        return fmt::format("{:0.3f} bytes / nanosecond", buf.size() / (double)ns);
    });

    auto build_sp = Signpost("mph", "building");
    build_sp.start();
    auto mph = MinimalPerfectHash<uint64_t>{};
    mph.build(keys);
    build_sp.end();

    return 0;
}
