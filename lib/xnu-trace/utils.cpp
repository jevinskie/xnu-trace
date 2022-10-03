#include "xnu-trace/utils.h"
#include "common-internal.h"

#include "xnu-trace/Atomic.h"
#include "xnu-trace/ThreadPool.h"
#include "xnu-trace/mach.h"
#include "xnu-trace/xnu-trace-c.h"

#include <atomic>
#include <cstring>
#include <sys/sysctl.h>

#include <mach/mach_error.h>
#include <mach/mach_init.h>

#include <mbedtls/sha256.h>
#include <zstd.h>

void posix_check(int retval, const std::string &msg) {
    if (XNUTRACE_UNLIKELY(retval)) {
        fmt::print(stderr, "POSIX error: '{:s}' retval: {:d} errno: {:d} description: '{:s}'\n",
                   msg, retval, errno, strerror(errno));
        if (get_task_for_pid_count(mach_task_self())) {
            XNUTRACE_BREAK();
        } else {
            exit(-1);
        }
    }
}

void mach_check(kern_return_t kr, const std::string &msg) {
    if (XNUTRACE_UNLIKELY(kr != KERN_SUCCESS)) {
        fmt::print(stderr, "Mach error: '{:s}' retval: {:d} description: '{:s}'\n", msg, kr,
                   mach_error_string(kr));
        if (get_task_for_pid_count(mach_task_self())) {
            XNUTRACE_BREAK();
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

void write_file(const std::string &path, const uint8_t *buf, size_t sz) {
    const auto fh = fopen(path.c_str(), "wb");
    assert(fh);
    assert(fwrite(buf, sz, 1, fh) == 1);
    assert(!fclose(fh));
}

std::vector<uint8_t> read_file(const std::string &path) {
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

void pipe_set_single_step(int do_ss) {
    const uint8_t wbuf = do_ss ? 'y' : 'n';
    assert(write(PIPE_TARGET2TRACER_FD, &wbuf, 1) == 1);
    uint8_t rbuf = 0;
    assert(read(PIPE_TRACER2TARGET_FD, &rbuf, 1) == 1);
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

std::string block_str(double percentage, unsigned int width) {
    const auto full_width                    = width * 8;
    const unsigned int num_blk               = percentage * full_width;
    const auto full_blks                     = num_blk / 8;
    const auto partial_blks                  = num_blk % 8;
    static const std::string partial_chars[] = {"", "▏", "▎", "▍", "▌", "▋", "▊", "▉"};
    std::string res;
    for (unsigned int i = 0; i < full_blks; ++i) {
        res += "█";
    }
    return res + partial_chars[partial_blks];
}

// ----------------------------------------------------------------------
// Copyright © 2005-2020 Rich Felker, et al.

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// ----------------------------------------------------------------------

#define BITOP(a, b, op)                                                                            \
    ((a)[(size_t)(b) / (8 * sizeof *(a))] op(size_t) 1 << ((size_t)(b) % (8 * sizeof *(a))))

static char *twoway_memmem(const unsigned char *h, const unsigned char *z, const unsigned char *n,
                           size_t l) {
    size_t i, ip, jp, k, p, ms, p0, mem, mem0;
    size_t byteset[32 / sizeof(size_t)] = {0};
    size_t shift[256];

    /* Computing length of needle and fill shift table */
    for (i = 0; i < l; i++)
        BITOP(byteset, n[i], |=), shift[n[i]] = i + 1;

    /* Compute maximal suffix */
    ip = -1;
    jp = 0;
    k = p = 1;
    while (jp + k < l) {
        if (n[ip + k] == n[jp + k]) {
            if (k == p) {
                jp += p;
                k = 1;
            } else
                k++;
        } else if (n[ip + k] > n[jp + k]) {
            jp += k;
            k = 1;
            p = jp - ip;
        } else {
            ip = jp++;
            k = p = 1;
        }
    }
    ms = ip;
    p0 = p;

    /* And with the opposite comparison */
    ip = -1;
    jp = 0;
    k = p = 1;
    while (jp + k < l) {
        if (n[ip + k] == n[jp + k]) {
            if (k == p) {
                jp += p;
                k = 1;
            } else
                k++;
        } else if (n[ip + k] < n[jp + k]) {
            jp += k;
            k = 1;
            p = jp - ip;
        } else {
            ip = jp++;
            k = p = 1;
        }
    }
    if (ip + 1 > ms + 1)
        ms = ip;
    else
        p = p0;

    /* Periodic needle? */
    if (memcmp(n, n + p, ms + 1)) {
        mem0 = 0;
        p    = MAX(ms, l - ms - 1) + 1;
    } else
        mem0 = l - p;
    mem = 0;

    /* Search loop */
    for (;;) {
        /* If remainder of haystack is shorter than needle, done */
        if ((size_t)(z - h) < l)
            return nullptr;

        /* Check last byte first; advance by shift on mismatch */
        if (BITOP(byteset, h[l - 1], &)) {
            k = l - shift[h[l - 1]];
            if (k) {
                if (k < mem)
                    k = mem;
                h += k;
                mem = 0;
                continue;
            }
        } else {
            h += l;
            mem = 0;
            continue;
        }

        /* Compare right half */
        for (k = MAX(ms + 1, mem); k < l && n[k] == h[k]; k++)
            ;
        if (k < l) {
            h += k - ms;
            mem = 0;
            continue;
        }
        /* Compare left half */
        for (k = ms + 1; k > mem && n[k - 1] == h[k - 1]; k--)
            ;
        if (k <= mem)
            return (char *)h;
        h += p;
        mem = mem0;
    }
}

void *horspool_memmem(const void *haystack, size_t haystack_sz, const void *needle,
                      size_t needle_sz) {
    const unsigned char *h = (unsigned char *)haystack, *n = (unsigned char *)needle;

    /* Return immediately on empty needle */
    if (!needle_sz)
        return (void *)h;

    /* Return immediately when needle is longer than haystack */
    if (haystack_sz < needle_sz)
        return 0;

    /* Use faster algorithms for short needles */
    h = (unsigned char *)memchr(haystack, *n, haystack_sz);
    if (!h || needle_sz == 1)
        return (void *)h;
    haystack_sz -= h - (const unsigned char *)haystack;
    if (haystack_sz < needle_sz)
        return nullptr;

    return twoway_memmem(h, h + haystack_sz, n, needle_sz);
}

std::vector<void *> chunk_into_bins_by_needle(uint32_t n, const void *haystack, size_t haystack_sz,
                                              const void *needle, size_t needle_sz) {
    std::vector<void *> res(n);
    const auto bin_sz       = haystack_sz / n;
    const auto haystack_end = (uint8_t *)haystack + haystack_sz;
    AtomicWaiter waiter(n);
    for (uint32_t i = 0; i < n; ++i) {
        xnutrace_pool.push_task([&, i] {
            const auto sub_haystack_begin = (uint8_t *)((uintptr_t)haystack + i * bin_sz);
            const auto sub_haystack_sz =
                std::min(bin_sz, (size_t)(haystack_end - sub_haystack_begin));
            res[i] = horspool_memmem(sub_haystack_begin, sub_haystack_sz, needle, needle_sz);
            waiter.increment();
        });
    }
    waiter.wait();
    return res;
}
