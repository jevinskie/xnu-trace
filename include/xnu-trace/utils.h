#pragma once

#include "common.h"

#include <array>
#include <bit>
#include <concepts>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <mach/machine/kern_return.h>
#include <mach/vm_prot.h>

#define MCA_BEGIN(name)                                                                            \
    do {                                                                                           \
        __asm volatile("# LLVM-MCA-BEGIN " #name ::: "memory");                                    \
    } while (0)
#define MCA_END()                                                                                  \
    do {                                                                                           \
        __asm volatile("# LLVM-MCA-END" ::: "memory");                                             \
    } while (0)

struct mbedtls_sha256_context;

using sha256_t = std::array<uint8_t, 32>;

template <typename T> size_t bytesizeof(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

template <typename T> constexpr bool is_pow2(T num) {
    return std::popcount(num) == 1;
}

// behavior:
// roundup_pow2_mul(16, 16) = 16
// roundup_pow2_mul(17, 16) = 32
template <typename U>
requires requires() {
    requires std::unsigned_integral<U>;
}
constexpr U roundup_pow2_mul(U num, size_t pow2_mul) {
    const U mask = static_cast<U>(pow2_mul) - 1;
    return (num + mask) & ~mask;
}

// behavior:
// roundup_pow2_mul(16, 16) = 16
// roundup_pow2_mul(17, 16) = 16
template <typename U>
requires requires() {
    requires std::unsigned_integral<U>;
}
constexpr U rounddown_pow2_mul(U num, size_t pow2_mul) {
    const U mask = static_cast<U>(pow2_mul) - 1;
    return num & ~mask;
}

XNUTRACE_EXPORT void posix_check(int retval, const std::string &msg);
XNUTRACE_EXPORT void mach_check(kern_return_t kr, const std::string &msg);

XNUTRACE_EXPORT uint32_t get_num_cores();

XNUTRACE_EXPORT void hexdump(const void *data, size_t size);

XNUTRACE_EXPORT std::vector<uint8_t> read_file(const std::string &path);
XNUTRACE_EXPORT void write_file(const std::string &path, const uint8_t *buf, size_t sz);

XNUTRACE_EXPORT double timespec_diff(const timespec &a, const timespec &b);
XNUTRACE_EXPORT std::string prot_to_str(vm_prot_t prot);
XNUTRACE_EXPORT std::string block_str(double percentage, unsigned int width = 80);

XNUTRACE_EXPORT sha256_t get_sha256(std::span<const uint8_t> buf);

class XNUTRACE_EXPORT SHA256 {
public:
    SHA256();
    ~SHA256();
    void update(std::span<const uint8_t> buf);
    sha256_t digest();

private:
    mbedtls_sha256_context *m_ctx;
    sha256_t m_digest;
    bool m_finished{};
};

XNUTRACE_EXPORT void *horspool_memmem(const void *haystack, size_t haystack_sz, const void *needle,
                                      size_t needle_sz);

XNUTRACE_EXPORT std::vector<void *> chunk_into_bins_by_needle(uint32_t n, const void *haystack,
                                                              size_t haystack_sz,
                                                              const void *needle, size_t needle_sz);

template <typename T> std::vector<T> read_numbers_from_file(const std::filesystem::path &path) {
    const auto buf     = read_file(path);
    const auto raw_buf = (T *)buf.data();
    const auto nnums   = buf.size() / sizeof(T);
    std::vector<T> nums;
    nums.reserve(nnums);
    for (size_t i = 0; i < nnums; ++i) {
        nums.emplace_back(raw_buf[i]);
    }
    return nums;
}

template <typename T> std::vector<T> get_random_scalars(size_t n) {
    std::vector<T> res;
    res.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        T r;
        arc4random_buf((uint8_t *)&r, sizeof(r));
        res.emplace_back(r);
    }
    return res;
}

template <typename T>
std::vector<T> get_random_sorted_unique_scalars(size_t min_sz, size_t max_sz) {
    std::vector<T> res;
    do {
        res = std::move(get_random_scalars<T>(max_sz));
        std::sort(res.begin(), res.end());
        res.erase(std::unique(res.begin(), res.end()), res.end());
    } while (res.size() < min_sz);
    return res;
}
