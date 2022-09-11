#include "xnu-single-step-trace/xnu-single-step-trace.h"

#undef NDEBUG
#include <cassert>
#include <cstdint>
#include <filesystem>

#include <argparse/argparse.hpp>
#include <fmt/format.h>

namespace fs = std::filesystem;

int main(int argc, const char **argv) {
    argparse::ArgumentParser parser(getprogname());

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        fmt::print(stderr, "Error parsing arguments: {:s}\n", err.what());
        return -1;
    }

    return 0;
}
