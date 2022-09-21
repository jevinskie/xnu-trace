#include "common.h"

#include <algorithm>

#include <zstd.h>

namespace jev::xnutrace::detail {

CompressedFile::CompressedFile(const fs::path &path, bool read, size_t hdr_sz, uint64_t hdr_magic,
                               const void *hdr, int level, bool verbose, int num_threads)
    : m_path{path}, m_is_read{read}, m_verbose{verbose} {
    if (read) {
        m_fh = fopen(path.c_str(), "rb");
        posix_check(!m_fh, fmt::format("can't open '{:s}", path.string()));
        log_comp_hdr comp_hdr;
        assert(fread(&comp_hdr, sizeof(comp_hdr), 1, m_fh) == 1);
        assert(comp_hdr.magic == hdr_magic || hdr_magic == UINT64_MAX);
        assert(comp_hdr.header_size == hdr_sz || hdr_sz == UINT64_MAX);
        m_hdr_sz      = comp_hdr.header_size;
        m_decomp_size = comp_hdr.decompressed_size;
        m_hdr_buf.resize(comp_hdr.header_size);
        assert(fread(m_hdr_buf.data(), comp_hdr.header_size, 1, m_fh) == 1);
        if (comp_hdr.is_compressed) {
            m_decomp_ctx = ZSTD_createDCtx();
            assert(m_decomp_ctx);
            m_in_buf.resize(ZSTD_DStreamInSize());
            m_out_buf.resize(ZSTD_DStreamOutSize());
        }
    } else {
        assert(hdr);
        m_hdr_sz = hdr_sz;
        m_hdr_buf.resize(hdr_sz);
        m_fh = fopen(path.c_str(), "wb");
        posix_check(!m_fh, fmt::format("can't open '{:s}", path.string()));
        log_comp_hdr comp_hdr{
            .magic = hdr_magic, .is_compressed = level != 0, .header_size = hdr_sz};
        assert(fwrite(&comp_hdr, sizeof(comp_hdr), 1, m_fh) == 1);
        memcpy(m_hdr_buf.data(), hdr, m_hdr_buf.size());
        assert(fwrite(hdr, hdr_sz, 1, m_fh) == 1);
        if (level) {
            m_comp_ctx = ZSTD_createCCtx();
            assert(m_comp_ctx);
            zstd_check(ZSTD_CCtx_setParameter(m_comp_ctx, ZSTD_c_compressionLevel, level),
                       "zstd set compression level");
            zstd_check(ZSTD_CCtx_setParameter(m_comp_ctx, ZSTD_c_checksumFlag, true),
                       "zstd enable checksums");
            if (num_threads < 1) {
                num_threads = get_num_cores();
            }
            zstd_check(ZSTD_CCtx_setParameter(m_comp_ctx, ZSTD_c_nbWorkers, num_threads),
                       "zstd set num threads");
            m_in_buf.resize(ZSTD_CStreamInSize());
            m_out_buf.resize(ZSTD_CStreamOutSize());
        }
    }
}

CompressedFile::~CompressedFile() {
    if (m_comp_ctx) {
        bool done = false;
        do {
            ZSTD_inBuffer input{.src = nullptr, .size = 0};
            ZSTD_outBuffer output{.dst = m_out_buf.data(), .size = m_out_buf.size()};
            const auto remaining = ZSTD_compressStream2(m_comp_ctx, &output, &input, ZSTD_e_end);
            zstd_check(remaining, "final write ZSTD_compressStream2");
            ++m_num_zstd_ops;
            if (output.pos) {
                assert(fwrite(m_out_buf.data(), output.pos, 1, m_fh) == 1);
                ++m_num_disk_ops;
            }
            done = remaining == 0;
        } while (!done);
        zstd_check(ZSTD_freeCCtx(m_comp_ctx), "zstd free comp ctx");
    } else if (m_decomp_ctx) {
        zstd_check(ZSTD_freeDCtx(m_decomp_ctx), "zstd free decomp ctx");
    }
    if (!m_is_read) {
        assert(!fseek(m_fh, offsetof(log_comp_hdr, decompressed_size), SEEK_SET));
        assert(fwrite(&m_decomp_size, sizeof(m_decomp_size), 1, m_fh) == 1);
        assert(!fseek(m_fh, sizeof(log_comp_hdr), SEEK_SET));
        assert(m_hdr_sz == m_hdr_buf.size());
        assert(fwrite(m_hdr_buf.data(), m_hdr_buf.size(), 1, m_fh) == 1);
        assert(!fseek(m_fh, 0, SEEK_END));
        const auto total_comp_sz = ftell(m_fh);
        assert(total_comp_sz > 0);
        const auto comp_sz = total_comp_sz - (sizeof(log_comp_hdr) + m_hdr_buf.size());
        if (m_verbose) {
            fmt::print(
                "{:s}\n",
                fmt::format(std::locale("en_US.UTF-8"),
                            "wrote '{:s}'. {:Ld} / {:Ld} bytes [un]/compressed ratio: {:0.3Lf}%",
                            m_path.filename().string(), m_decomp_size, comp_sz,
                            ((double)comp_sz / m_decomp_size) * 100));
            fmt::print("{:s}\n",
                       fmt::format(std::locale("en_us.UTF-8"),
                                   "Decompressed bytes / file op: {:0.3Lf}",
                                   m_num_disk_ops ? (double)m_decomp_size / m_num_disk_ops : 0.0));
            fmt::print("{:s}\n",
                       fmt::format(std::locale("en_us.UTF-8"),
                                   "Decompressed bytes / zstd op: {:0.3Lf}",
                                   m_num_zstd_ops ? (double)m_decomp_size / m_num_zstd_ops : 0.0));
            fmt::print("{:s}\n",
                       fmt::format(std::locale("en_us.UTF-8"),
                                   "Ccompressed bytes / file op: {:0.3Lf}",
                                   m_num_disk_ops ? (double)comp_sz / m_num_disk_ops : 0.0));
            fmt::print("{:s}\n",
                       fmt::format(std::locale("en_us.UTF-8"),
                                   "Compressed bytes / zstd op: {:0.3Lf}",
                                   m_num_zstd_ops ? (double)comp_sz / m_num_zstd_ops : 0.0));
        }
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
        ++m_num_disk_ops;
    } else {
        auto to_read        = size;
        auto suggested_read = m_in_buf.size();
        auto *out_ptr       = buf;
        while (to_read) {
            const auto comp_read = fread(m_in_buf.data(), 1, suggested_read, m_fh);
            ++m_num_disk_ops;
            assert(comp_read > 0);
            ZSTD_inBuffer input{.src = m_in_buf.data(), .size = comp_read};
            while (input.pos < input.size) {
                ZSTD_outBuffer output{.dst = m_out_buf.data(), .size = m_out_buf.size()};
                suggested_read = ZSTD_decompressStream(m_decomp_ctx, &output, &input);
                zstd_check(suggested_read, "read ZSTD_decompressStream");
                ++m_num_zstd_ops;
                if (output.pos) {
                    memcpy(out_ptr, m_out_buf.data(), output.pos);
                }
                out_ptr += output.pos;
                to_read -= output.pos;
            }
        }
    }
    m_decomp_size += size;
}

void CompressedFile::write(std::span<const uint8_t> buf) {
    assert(!m_is_read);
    if (!m_comp_ctx) {
        assert(fwrite(buf.data(), buf.size(), 1, m_fh) == 1);
        ++m_num_disk_ops;
    } else {
        ZSTD_inBuffer input{.src = buf.data(), .size = buf.size()};
        bool done = false;
        do {
            ZSTD_outBuffer output{.dst = m_out_buf.data(), .size = m_out_buf.size()};
            const auto remaining =
                ZSTD_compressStream2(m_comp_ctx, &output, &input, ZSTD_e_continue);
            zstd_check(remaining, "write ZSTD_compressStream2");
            ++m_num_zstd_ops;
            if (output.pos) {
                assert(fwrite(m_out_buf.data(), output.pos, 1, m_fh) == 1);
                ++m_num_disk_ops;
            }
            done = input.pos == input.size;
        } while (!done);
    }
    m_decomp_size += buf.size();
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

size_t CompressedFile::decompressed_size() const {
    return m_decomp_size;
}

const std::vector<uint8_t> &CompressedFile::header_buf() const {
    return m_hdr_buf;
}

} // namespace jev::xnutrace::detail
