#include "common.h"

void posix_check(int retval, std::string msg) {
    if (retval) {
        fmt::print(stderr, "Error: '{:s}' retval: {:d} errno: {:d} description: '{:s}'\n", msg,
                   retval, errno, strerror(errno));
        if (get_task_for_pid_count(mach_task_self())) {
            __builtin_debugtrap();
        } else {
            exit(-1);
        }
    }
}

void mach_check(kern_return_t kr, std::string msg) {
    if (kr != KERN_SUCCESS) {
        fmt::print(stderr, "Error: '{:s}' retval: {:d} description: '{:s}'\n", msg, kr,
                   mach_error_string(kr));
        if (get_task_for_pid_count(mach_task_self())) {
            __builtin_debugtrap();
        } else {
            exit(-1);
        }
    }
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
