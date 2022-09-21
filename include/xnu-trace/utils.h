#pragma once

#include "common.h"

#include <array>
#include <concepts>
#include <span>

#include <mach/machine/kern_return.h>
#include <mach/vm_prot.h>

struct mbedtls_sha256_context;

using sha256_t = std::array<uint8_t, 32>;

template <typename T> size_t bytesizeof(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

template <typename U>
requires requires() {
    requires std::unsigned_integral<U>;
}
constexpr U roundup_pow2_mul(U num, std::size_t pow2_mul) {
    const U mask = static_cast<U>(pow2_mul) - 1;
    return (num + mask) & ~mask;
}

template <typename U>
requires requires() {
    requires std::unsigned_integral<U>;
}
constexpr U rounddown_pow2_mul(U num, std::size_t pow2_mul) {
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
