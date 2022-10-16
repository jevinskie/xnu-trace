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

struct Number {
    Number(int num) {
        assert(num >= 0 && num < 4);
        switch (num) {
        case 0:
            m_bv = std::make_unique<NumberDerived<0>>();
            break;
        case 1:
            m_bv = std::make_unique<NumberDerived<1>>();
            break;
        case 2:
            m_bv = std::make_unique<NumberDerived<2>>();
            break;
        case 3:
            m_bv = std::make_unique<NumberDerived<3>>();
            break;
        default:
            __builtin_unreachable();
        }
    }
    void print(int addend) const {
        m_bv->print(addend);
    }
    std::unique_ptr<NumberBase> m_bv;
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
