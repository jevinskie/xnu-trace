#undef NDEBUG
#include <cassert>
#include <spawn.h>
#include <sys/wait.h>

#ifndef _POSIX_SPAWN_DISABLE_ASLR
#define _POSIX_SPAWN_DISABLE_ASLR 0x0100
#endif

extern "C" char **environ;

int main(int argc, char **argv) {
    assert(argc >= 2);

    posix_spawnattr_t attr;
    assert(!posix_spawnattr_init(&attr));
    assert(!posix_spawnattr_setflags(&attr, _POSIX_SPAWN_DISABLE_ASLR));

    pid_t pid;
    assert(!posix_spawnp(&pid, argv[1], nullptr, &attr, &argv[1], environ));
    assert(!posix_spawnattr_destroy(&attr));
    int status;
    while (waitpid(pid, &status, 0) >= 0) {
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -41;
    }
    return -42;
}
