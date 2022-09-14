#include "common.h"

#include <sys/sysctl.h>

#include <mbedtls/sha256.h>
#include <zstd.h>

void posix_check(int retval, const std::string &msg) {
    if (retval) {
        fmt::print(stderr, "POSIX error: '{:s}' retval: {:d} errno: {:d} description: '{:s}'\n",
                   msg, retval, errno, strerror(errno));
        if (get_task_for_pid_count(mach_task_self())) {
            __builtin_debugtrap();
        } else {
            exit(-1);
        }
    }
}

void mach_check(kern_return_t kr, const std::string &msg) {
    if (kr != KERN_SUCCESS) {
        fmt::print(stderr, "Mach error: '{:s}' retval: {:d} description: '{:s}'\n", msg, kr,
                   mach_error_string(kr));
        if (get_task_for_pid_count(mach_task_self())) {
            __builtin_debugtrap();
        } else {
            exit(-1);
        }
    }
}

void zstd_check(size_t retval, const std::string &msg) {
    if (ZSTD_isError(retval)) {
        fmt::print(stderr, "Zstd error: '{:s}' retval: {:#018x} description: '{:s}'\n", msg, retval,
                   ZSTD_getErrorName(retval));
        if (get_task_for_pid_count(mach_task_self())) {
            __builtin_debugtrap();
        } else {
            exit(-1);
        }
    }
}

uint32_t get_num_cores() {
    uint32_t num;
    size_t sz = sizeof(num);
    posix_check(sysctlbyname("hw.logicalcpu", &num, &sz, nullptr, 0), "get num cores");
    return num;
}

double timespec_diff(const timespec &a, const timespec &b) {
    timespec c;
    c.tv_sec  = a.tv_sec - b.tv_sec;
    c.tv_nsec = a.tv_nsec - b.tv_nsec;
    if (c.tv_nsec < 0) {
        --c.tv_sec;
        c.tv_nsec += 1'000'000'000L;
    }
    return c.tv_sec + ((double)c.tv_nsec / 1'000'000'000L);
}

void write_file(std::string path, const uint8_t *buf, size_t sz) {
    const auto fh = fopen(path.c_str(), "wb");
    assert(fh);
    assert(fwrite(buf, sz, 1, fh) == 1);
    assert(!fclose(fh));
}

std::vector<uint8_t> read_file(std::string path) {
    std::vector<uint8_t> res;
    const auto fh = fopen(path.c_str(), "rb");
    assert(fh);
    assert(!fseek(fh, 0, SEEK_END));
    const auto sz = ftell(fh);
    assert(!fseek(fh, 0, SEEK_SET));
    res.resize(sz);
    assert(fread(res.data(), sz, 1, fh) == 1);
    assert(!fclose(fh));
    return res;
}

std::string prot_to_str(vm_prot_t prot) {
    std::string s;
    s += prot & VM_PROT_READ ? "r" : "-";
    s += prot & VM_PROT_WRITE ? "w" : "-";
    s += prot & VM_PROT_EXECUTE ? "x" : "-";
    s += prot & ~(VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE) ? "*" : " ";
    return s;
}

// https://gist.github.com/ccbrown/9722406
void hexdump(const void *data, size_t size) {
    char ascii[17];
    size_t i, j;
    ascii[16] = '\0';
    for (i = 0; i < size; ++i) {
        printf("%02X ", ((unsigned char *)data)[i]);
        if (((unsigned char *)data)[i] >= ' ' && ((unsigned char *)data)[i] <= '~') {
            ascii[i % 16] = ((unsigned char *)data)[i];
        } else {
            ascii[i % 16] = '.';
        }
        if ((i + 1) % 8 == 0 || i + 1 == size) {
            printf(" ");
            if ((i + 1) % 16 == 0) {
                printf("|  %s \n", ascii);
            } else if (i + 1 == size) {
                ascii[(i + 1) % 16] = '\0';
                if ((i + 1) % 16 <= 8) {
                    printf(" ");
                }
                for (j = (i + 1) % 16; j < 16; ++j) {
                    printf("   ");
                }
                printf("|  %s \n", ascii);
            }
        }
    }
}

void pipe_set_single_step(bool do_ss) {
    const uint8_t wbuf = do_ss ? 'y' : 'n';
    assert(write(pipe_target2tracer_fd, &wbuf, 1) == 1);
    uint8_t rbuf = 0;
    assert(read(pipe_tracer2target_fd, &rbuf, 1) == 1);
    assert(rbuf == 'c');
}

sha256_t get_sha256(std::span<const uint8_t> buf) {
    sha256_t hash;
    assert(!mbedtls_sha256(buf.data(), buf.size(), hash.data(), false));
    return hash;
}

SHA256::SHA256() {
    m_ctx = new mbedtls_sha256_context{};
    mbedtls_sha256_init(m_ctx);
    assert(!mbedtls_sha256_starts(m_ctx, 0));
}

SHA256::~SHA256() {
    mbedtls_sha256_free(m_ctx);
    delete m_ctx;
}

void SHA256::update(std::span<const uint8_t> buf) {
    assert(!m_finished);
    assert(!mbedtls_sha256_update(m_ctx, buf.data(), buf.size()));
}

sha256_t SHA256::digest() {
    if (m_finished) {
        return m_digest;
    }
    assert(!mbedtls_sha256_finish(m_ctx, m_digest.data()));
    m_finished = true;
    return m_digest;
}
