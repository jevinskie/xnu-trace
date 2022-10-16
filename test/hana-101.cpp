#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>

struct NumberBase {
    virtual ~NumberBase() {}
    virtual void print(int addend) const = 0;
};

template <int Num> struct NumberDerived : NumberBase {
    void print(int addend) const final override {
        printf("NumberDerived is %d\n", Num + addend);
    }
};

std::unique_ptr<NumberBase> NumberFactory(int num) {
    assert(num >= 0 && num < 4);
    switch (num) {
    case 0:
        return std::make_unique<NumberDerived<0>>();
    case 1:
        return std::make_unique<NumberDerived<1>>();
    case 2:
        return std::make_unique<NumberDerived<2>>();
    case 3:
        return std::make_unique<NumberDerived<3>>();
    default:
        return {};
    }
}

struct Number {
    Number(int num) : m_bv(NumberFactory(num)) {}
    __attribute__((always_inline)) void print(int addend) const {
        m_bv->print(addend);
    }
    const std::unique_ptr<NumberBase> m_bv;
};

__attribute__((noinline)) void call_print_bad(const Number &number) {
    for (int i = 0; i < 4; ++i) {
        number.print(i);
    }
}

__attribute__((noinline)) void call_print_good(const Number &number) {
    const auto bv_impl = number.m_bv.get();
    for (int i = 0; i < 4; ++i) {
        bv_impl->print(i);
    }
}

int main(int argc, const char **argv) {
    assert(argc == 2);
    int n = atoi(argv[1]);

    const auto num = Number(n);
    call_print_bad(num);
    call_print_good(num);

    return 0;
}
