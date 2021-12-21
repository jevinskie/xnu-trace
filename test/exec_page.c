#include <errno.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

__attribute__((always_inline))
ssize_t write_asm(int fid, void *buf, size_t nbyte)
{
    // If read() is interrupted by a signal, retry.
    long sys_result;
    do {
        register long _fid __asm("x0") = (long)fid;
        const register void* _buf __asm("x1") = buf;
        const register size_t _nbyte __asm("x2") = nbyte;
        const register int _sc_num __asm("x16") = SYS_write;        
        __asm __volatile (
            "svc 0x80             \n\t"
            : "=r"(_fid)
            : "r"(_sc_num), "r"(_fid), "r"(_buf), "r"(_nbyte)
            : "memory"
        );
        sys_result = _fid;
    } while (-sys_result == EINTR);

    return (ssize_t)sys_result;
}

__attribute__((flatten))
void always_exec(void) {
    write_asm(STDERR_FILENO, "hello\n", sizeof("hello\n"));
}

int main(void) {
    fprintf(stderr, "main begin\n");
    always_exec();
    fprintf(stderr, "main end\n");
}
