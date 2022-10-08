#include <cassert>
#include <cstdlib>

#include <boost/hana.hpp>
namespace hana = boost::hana;

#include <fmt/format.h>

struct NumberBase {
    virtual ~NumberBase() {}
    virtual void print() const = 0;
};

template <int Num> struct NumberDerived : NumberBase {
    void print() const final override {
        fmt::print("NumberDerived is {:d}\n", Num);
    }
};

struct Number {
    Number(int num) {
        assert(num >= 0 && num < 64);
        hana::for_each(hana::to_tuple(hana::range_c<int, 0, 64>), [&](auto x) {
            if (x.value == num) {
                m_bv = std::make_unique<NumberDerived<x.value>>();
            }
        });
    }
    void print() const {
        m_bv->print();
    }
    std::unique_ptr<NumberBase> m_bv;
};

int main(int argc, const char **argv) {
    assert(argc == 2);
    int n = atoi(argv[1]);

    const auto num = Number(n);
    num.print();

    return 0;
}
