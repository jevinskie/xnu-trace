#pragma once

#include "common.h"

#include <filesystem>
#include <span>
#include <vector>

struct ZSTD_CCtx_s;
struct ZSTD_DCtx_s;

namespace jev::xnutrace::detail {

class XNUTRACE_EXPORT CompressedFile {
public:
    CompressedFile(const std::filesystem::path &path, bool read, size_t hdr_sz, uint64_t hdr_magic,
                   const void *hdr = nullptr, int level = 3, bool verbose = false,
                   int num_threads = 0);
    ~CompressedFile();

    template <typename T> const T &header() const {
        assert(sizeof(T) == m_hdr_buf.size());
        return *(T *)m_hdr_buf.data();
    };
    template <typename T> T &header() {
        return const_cast<T &>(std::as_const(*this).header<T>());
    };
    const std::vector<uint8_t> &header_buf() const;
    std::vector<uint8_t> &header_buf() {
        return const_cast<std::vector<uint8_t> &>(std::as_const(*this).header_buf());
    }

    std::vector<uint8_t> read();
    std::vector<uint8_t> read(size_t size);
    void read(uint8_t *buf, size_t size);
    template <typename T>
    requires POD<T> T read() {
        T buf;
        read((uint8_t *)&buf, sizeof(T));
        return buf;
    }

    void write(std::span<const uint8_t> buf);
    void write(const void *buf, size_t size);
    void write(const uint8_t *buf, size_t size);
    void write(const char *buf, size_t size);
    template <typename T>
    requires POD<T>
    void write(const T &buf) {
        write({(uint8_t *)&buf, sizeof(buf)});
    }

    size_t decompressed_size() const;

private:
    const std::filesystem::path m_path;
    FILE *m_fh{};
    ZSTD_CCtx_s *m_comp_ctx{};
    std::vector<uint8_t> m_in_buf;
    std::vector<uint8_t> m_out_buf;
    ZSTD_DCtx_s *m_decomp_ctx{};
    bool m_is_read{};
    bool m_verbose{};
    std::vector<uint8_t> m_hdr_buf;
    size_t m_decomp_size{};
    uint64_t m_num_disk_ops{};
    uint64_t m_num_zstd_ops{};
    size_t m_hdr_sz{};
};

} // namespace jev::xnutrace::detail

template <typename HeaderT>
class XNUTRACE_EXPORT CompressedFile : public jev::xnutrace::detail::CompressedFile {
public:
    CompressedFile(const std::filesystem::path &path, bool read, uint64_t hdr_magic,
                   const HeaderT *hdr = nullptr, int level = 3, bool verbose = false)
        : jev::xnutrace::detail::CompressedFile::CompressedFile{
              path, read, sizeof(HeaderT), hdr_magic, hdr, level, verbose} {};

    const HeaderT &header() const {
        return jev::xnutrace::detail::CompressedFile::header<HeaderT>();
    }
    HeaderT &header() {
        return const_cast<HeaderT &>(std::as_const(*this).header());
    }
};

class XNUTRACE_EXPORT CompressedFileRawRead : public jev::xnutrace::detail::CompressedFile {
public:
    CompressedFileRawRead(const std::filesystem::path &path)
        : jev::xnutrace::detail::CompressedFile::CompressedFile{path, true, UINT64_MAX,
                                                                UINT64_MAX} {};
};
