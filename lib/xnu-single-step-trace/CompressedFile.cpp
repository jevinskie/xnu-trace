#include "common.h"

CompressedFile::CompressedFile(const std::filesystem::path &path, bool read, int ratio) {
    return;
}

std::vector<uint8_t> CompressedFile::read() {
    return {};
}

void CompressedFile::write(std::span<const uint8_t> buf) {
    return;
}

void CompressedFile::write(const void *buf, size_t size) {
    write({(uint8_t *)buf, size});
}

void CompressedFile::write(const uint8_t *buf, size_t size) {
    write({buf, size});
}
