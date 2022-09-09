#include "common.h"

#include <algorithm>

#include <zstd.h>

CompressedFile::CompressedFile(const fs::path &path, bool read, size_t hdr_sz, uint64_t hdr_magic,
                               const void *hdr, int level)
    : m_is_read{read} {
    if (read) {
        m_fh = fopen(path.c_str(), "rb");
        assert(m_fh);
        log_comp_hdr comp_hdr;
        assert(fread(&comp_hdr, sizeof(comp_hdr), 1, m_fh) == 1);
        assert(comp_hdr.magic == hdr_magic);
        m_decomp_size = comp_hdr.decompressed_size;
        m_hdr_buf.resize(hdr_sz);
        assert(fread(m_hdr_buf.data(), hdr_sz, 1, m_fh) == 1);
        if (comp_hdr.is_compressed) {
            m_decomp_ctx = ZSTD_createDCtx();
            assert(m_decomp_ctx);
        }
    } else {
        assert(hdr);
        m_fh = fopen(path.c_str(), "wb");
        assert(m_fh);
        log_comp_hdr comp_hdr{.magic = hdr_magic, .is_compressed = level != 0};
        assert(fwrite(&comp_hdr, sizeof(comp_hdr), 1, m_fh) == 1);
        if (level) {
            m_comp_ctx = ZSTD_createCCtx();
            assert(m_comp_ctx);
            zstd_check(ZSTD_CCtx_setParameter(m_comp_ctx, ZSTD_c_compressionLevel, level),
                       "zstd set compression level");
            zstd_check(ZSTD_CCtx_setParameter(m_comp_ctx, ZSTD_c_checksumFlag, true),
                       "zstd enable checksums");
            zstd_check(ZSTD_CCtx_setParameter(m_comp_ctx, ZSTD_c_nbWorkers,
                                              std::min(1u, get_num_cores() / 2)),
                       "zstd set num threads");
        }
    }
    return;
}

CompressedFile::~CompressedFile() {
    if (!m_is_read) {
        assert(!fseek(m_fh, offsetof(log_comp_hdr, decompressed_size), SEEK_SET));
        assert(fwrite(&m_decomp_size, sizeof(m_decomp_size), 1, m_fh) == 1);
    }
    assert(!fclose(m_fh));
}

std::vector<uint8_t> CompressedFile::read() {
    return read(m_decomp_size);
}

std::vector<uint8_t> CompressedFile::read(size_t size) {
    assert(m_is_read);
    std::vector<uint8_t> buf(size);
    if (!m_comp_ctx) {
        assert(fread(buf.data(), size, 1, m_fh) == 1);
    } else {
        assert(!"not implemented");
    }
    return buf;
}

void CompressedFile::read(uint8_t *buf, size_t size) {
    if (!m_comp_ctx) {
        assert(fread(buf, size, 1, m_fh) == 1);
    } else {
        assert(!"not implemented");
    }
}

void CompressedFile::write(std::span<const uint8_t> buf) {
    assert(!m_is_read);
    if (!m_comp_ctx) {
        assert(fwrite(buf.data(), buf.size(), 1, m_fh) == 1);
    } else {
        assert(!"not implemented");
    }
    m_decomp_size += buf.size();
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
