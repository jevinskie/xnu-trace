#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <string_view>

#include <boost/hana.hpp>
namespace hana = boost::hana;
using namespace hana::literals;

#include <fmt/format.h>

template <typename T> constexpr auto type_name() {
    std::string_view name, prefix, suffix;
#ifdef __clang__
    name   = __PRETTY_FUNCTION__;
    prefix = "auto type_name() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name   = __PRETTY_FUNCTION__;
    prefix = "constexpr auto type_name() [with T = ";
    suffix = "]";
#elif defined(_MSC_VER)
    name   = __FUNCSIG__;
    prefix = "auto __cdecl type_name<";
    suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

template <int Num> struct Number {
    void print() {
        fmt::print("{:d}\n", Num);
    }
};

auto to_string = [](auto x) {
    std::ostringstream ss;
    ss << x;
    return ss.str();
};

int main(int argc, const char **argv) {
    assert(argc == 2);
    int n = atoi(argv[1]);

    const auto rng = hana::range_c<int, 0, 64>;

    const auto res = hana::transform(hana::to_tuple(rng), to_string);

    fmt::print("res type: {:s}\n", type_name<decltype(res)>());

    // for (size_t i = 0; i < hana::size(res); ++i) {
    //     fmt::print("r: {:s}\n", hana::at(res, hana::to_tuple(rng)[i]));
    // }

    hana::int_<5>::times.with_index([&](auto index) {
        fmt::print("idx: {:d}\n", index);
    });

    Number<243>().print();

    return 0;
}
