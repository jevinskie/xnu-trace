#include "common.h"

#include <bxzstr.hpp>

CompressedFile::CompressedFile(const std::filesystem::path &path, bool read, int level) {
    if (read) {
        m_ifstream =
            std::make_unique<bxz::ifstream>(path.string(), std::ios::in | std::ios_base::binary);
    } else {
        if (level) {
            m_ofstream = std::make_unique<bxz::ofstream>(
                path.string(), std::ios::out | std::ios_base::binary, bxz::zstd, level);
        } else {
            m_ofstream = std::make_unique<bxz::ofstream>(
                path.string(), std::ios::out | std::ios_base::binary, bxz::plaintext, 0);
        }
    }
    return;
}

std::vector<uint8_t> CompressedFile::read() {
    assert(m_ifstream);
    return {std::istreambuf_iterator<char>{*m_ifstream}, std::istreambuf_iterator<char>{}};
}

std::vector<uint8_t> CompressedFile::read(size_t size) {
    assert(m_ifstream);
    std::vector<uint8_t> buf(size, 0);
    m_ifstream->read((char *)buf.data(), size);
    return buf;
}

void CompressedFile::write(std::span<const uint8_t> buf) {
    assert(m_ofstream);
    m_ofstream->write((const char *)buf.data(), buf.size());
    return;
}

void CompressedFile::write(const void *buf, size_t size) {
    write({(uint8_t *)buf, size});
}

void CompressedFile::write(const uint8_t *buf, size_t size) {
    write({buf, size});
}
