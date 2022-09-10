#include "common.h"

#include <algorithm>

#include <zstd.h>

namespace jev::xnutrace::detail {

CompressedFile::CompressedFile(const fs::path &path, bool read, size_t hdr_sz, uint64_t hdr_magic,
                               const void *hdr, int level)
    : m_is_read{read} {
    if (read) {
        m_fh = fopen(path.c_str(), "rb");
        posix_check(!m_fh, fmt::format("can't open '{:s}", path.string()));
        if (level) {
            posix_check(setvbuf(m_fh, nullptr, _IONBF, 0), "compressed stream buffer disable");
        }
        log_comp_hdr comp_hdr;
        assert(fread(&comp_hdr, sizeof(comp_hdr), 1, m_fh) == 1);
        assert(comp_hdr.magic == hdr_magic);
        m_decomp_size = comp_hdr.decompressed_size;
        m_hdr_buf.resize(hdr_sz);
        assert(fread(m_hdr_buf.data(), hdr_sz, 1, m_fh) == 1);
        if (comp_hdr.is_compressed) {
            m_decomp_ctx = ZSTD_createDCtx();
            assert(m_decomp_ctx);
            m_in_buf.resize(ZSTD_DStreamInSize());
            m_out_buf.resize(ZSTD_DStreamOutSize());
        }
    } else {
        assert(hdr);
        m_fh = fopen(path.c_str(), "wb");
        posix_check(!m_fh, fmt::format("can't open '{:s}", path.string()));
        if (level) {
            posix_check(setvbuf(m_fh, nullptr, _IONBF, 0), "compressed stream buffer disable");
        }
        log_comp_hdr comp_hdr{.magic = hdr_magic, .is_compressed = level != 0};
        assert(fwrite(&comp_hdr, sizeof(comp_hdr), 1, m_fh) == 1);
        assert(fwrite(hdr, hdr_sz, 1, m_fh) == 1);
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
            m_in_buf.resize(ZSTD_CStreamInSize());
            m_out_buf.resize(ZSTD_CStreamOutSize());
        }
    }
    fmt::print("zstd buf sizes in/out: {:0.3f} / {:0.3f} KB\n", m_in_buf.size() / 1024.0,
               m_out_buf.size() / 1024.0);
}

CompressedFile::~CompressedFile() {
    if (m_comp_ctx) {
        ZSTD_inBuffer input{.src = nullptr, .size = 0};
        ZSTD_outBuffer output{.dst = m_out_buf.data(), .size = m_out_buf.size()};
        const auto remaining = ZSTD_compressStream2(m_comp_ctx, &output, &input, ZSTD_e_end);
        zstd_check(remaining, "final write ZSTD_compressStream2");
        assert(remaining == 0);
        if (output.pos) {
            assert(fwrite(m_out_buf.data(), output.pos, 1, m_fh) == 1);
        }
        zstd_check(ZSTD_freeCCtx(m_comp_ctx), "zstd free comp ctx");
    } else if (m_decomp_ctx) {
        zstd_check(ZSTD_freeDCtx(m_decomp_ctx), "zstd free decomp ctx");
    }
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
    std::vector<uint8_t> buf(size);
    read(buf.data(), size);
    return buf;
}

void CompressedFile::read(uint8_t *buf, size_t size) {
    assert(m_is_read);
    if (!m_decomp_ctx) {
        assert(fread(buf, size, 1, m_fh) == 1);
    } else {
        auto to_read        = size;
        auto suggested_read = m_in_buf.size();
        auto *out_ptr       = buf;
        while (to_read) {
            const auto comp_read = fread(m_in_buf.data(), 1, suggested_read, m_fh);
            assert(comp_read > 0);
            ZSTD_inBuffer input{.src = m_in_buf.data(), .size = comp_read};
            while (input.pos < input.size) {
                ZSTD_outBuffer output{.dst = m_out_buf.data(), .size = m_out_buf.size()};
                suggested_read = ZSTD_decompressStream(m_decomp_ctx, &output, &input);
                zstd_check(suggested_read, "read ZSTD_decompressStream");
                memcpy(out_ptr, m_out_buf.data(), output.pos);
                out_ptr += output.pos;
                to_read -= output.pos;
            }
        }
        assert(suggested_read == 0);
    }
}

void CompressedFile::write(std::span<const uint8_t> buf) {
    assert(!m_is_read);
    if (!m_comp_ctx) {
        assert(fwrite(buf.data(), buf.size(), 1, m_fh) == 1);
    } else {
        ZSTD_inBuffer input{.src = buf.data(), .size = buf.size()};
        bool done = false;
        do {
            ZSTD_outBuffer output{.dst = m_out_buf.data(), .size = m_out_buf.size()};
            const auto remaining =
                ZSTD_compressStream2(m_comp_ctx, &output, &input, ZSTD_e_continue);
            zstd_check(remaining, "write ZSTD_compressStream2");
            if (output.pos) {
                assert(fwrite(m_out_buf.data(), output.pos, 1, m_fh) == 1);
            }
            done = input.pos == input.size;
        } while (!done);
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

} // namespace jev::xnutrace::detail
