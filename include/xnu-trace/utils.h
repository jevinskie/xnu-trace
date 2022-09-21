#pragma once

#include "common.h"

struct mbedtls_sha256_context;

using sha256_t = std::array<uint8_t, 32>;

XNUTRACE_EXPORT void posix_check(int retval, const std::string &msg);
XNUTRACE_EXPORT void mach_check(kern_return_t kr, const std::string &msg);

XNUTRACE_EXPORT uint32_t get_num_cores();

XNUTRACE_EXPORT void hexdump(const void *data, size_t size);
XNUTRACE_EXPORT std::vector<uint8_t> read_file(const std::string &path);
XNUTRACE_EXPORT void write_file(const std::string &path, const uint8_t *buf, size_t sz);

XNUTRACE_EXPORT void pipe_set_single_step(bool do_ss);

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
