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
    parser.add_argument("-i", "--input").required().help("input CompressedFile path");
    parser.add_argument("-o", "--output").required().help("output path");
    parser.add_argument("-H", "--header")
        .default_value(false)
        .implicit_value(true)
        .help("output header instead of body");

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error &err) {
        fmt::print(stderr, "Error parsing arguments: {:s}\n", err.what());
        return -1;
    }

    const fs::path in_path{parser.get("--input")};
    const auto out_path{parser.get("--output")};
    const bool output_header{parser["--header"] == true};

    CompressedFileRawRead cf{in_path};

    if (output_header) {
        write_file(out_path, cf.header_buf().data(), cf.header_buf().size());
    } else {
        const auto buf = cf.read();
        write_file(out_path, buf.data(), buf.size());
    }

    return 0;
}
