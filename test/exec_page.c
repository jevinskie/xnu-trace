#include <errno.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#pragma clang attribute push(__attribute__((section("__TEXT,__always_exec"))), apply_to = function)
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_IMPLEMENTATION
#include "../3rdparty/nanoprintf/nanoprintf.h"
#pragma clang attribute pop

#pragma clang attribute push(__attribute__((section("__TEXT,__always_exec"))), apply_to = function)

ssize_t write_asm(int fid, void *buf, size_t nbyte) {
    // If write() is interrupted by a signal, retry.
    do {
        register long _fid __asm("x0")           = (long)fid;
        const register void *_buf __asm("x1")    = buf;
        const register size_t _nbyte __asm("x2") = nbyte;
        const register int _sc_num __asm("x16")  = SYS_write;
        __asm __volatile("svc 0x80             \n\t"
                         : "=r"(_fid)
                         : "r"(_sc_num), "r"(_fid), "r"(_buf), "r"(_nbyte)
                         : "memory");
    } while (-fid == EINTR);

    return (ssize_t)fid;
}

int nfprintf(FILE *f, char const *fmt, ...) {
    char buf[4096];
    va_list val;
    int fd;
    if (f == stdout) {
        fd = STDOUT_FILENO;
    } else {
        fd = STDERR_FILENO;
    }
    va_start(val, fmt);
    int const rv = npf_vsnprintf(buf, sizeof(buf), fmt, val);
    va_end(val);
    write_asm(fd, buf, rv);
    return rv;
}

void always_exec(void) {
    write_asm(STDERR_FILENO, "hello\n", sizeof("hello\n"));
}

#pragma clang attribute pop

int main(void) {
    fprintf(stderr, "main begin\n");
    always_exec();

    fprintf(stderr, "always_exec: %p\n", always_exec);
    nfprintf(stderr, "always_exec: %p\n", always_exec);

    fprintf(stderr, "main end\n");
}
