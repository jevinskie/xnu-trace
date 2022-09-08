#include "common.h"

#include <bxzstr.hpp>
#include <zstd.h>

size_t zstd_decompressed_size(std::span<const uint8_t> buf_head) {
    assert(buf_head.size() >= 18); // ZSTD_FRAMEHEADERSIZE_MAX
    const auto sz = ZSTD_getFrameContentSize((void *)buf_head.data(), buf_head.size());
    assert(sz != ZSTD_CONTENTSIZE_UNKNOWN && sz != ZSTD_CONTENTSIZE_ERROR);
    return sz;
}

size_t zstd_decompressed_size(const fs::path &zstd_path) {
    uint8_t buf[18]; // ZSTD_FRAMEHEADERSIZE_MAX
    auto *fh = fopen(zstd_path.c_str(), "rb");
    assert(fh);
    assert(fread((void *)buf, sizeof(buf), 1, fh) == 1);
    assert(!fclose(fh));
    return zstd_decompressed_size(buf);
}

CompressedFile::CompressedFile(const fs::path &path, bool read, int level) {
    if (read) {
        m_decomp_size = zstd_decompressed_size(path);
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

CompressedFile::~CompressedFile() = default;

std::vector<uint8_t> CompressedFile::read() {
    assert(m_ifstream);
    std::vector<uint8_t> buf(m_decomp_size);
    std::copy(std::istreambuf_iterator<char>{*m_ifstream}, std::istreambuf_iterator<char>{},
              std::back_inserter(buf));
    return buf;
}

std::vector<uint8_t> CompressedFile::read(size_t size) {
    assert(m_ifstream);
    std::vector<uint8_t> buf(size);
    m_ifstream->read((char *)buf.data(), size);
    return buf;
}

void CompressedFile::read(uint8_t *buf, size_t size) {
    assert(m_ifstream);
    m_ifstream->read((char *)buf, size);
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

void CompressedFile::write(const char *buf, size_t size) {
    write({(uint8_t *)buf, size});
}
