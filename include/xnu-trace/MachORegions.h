#pragma once

#include "common.h"

struct image_info {
    uint64_t base;
    uint64_t size;
    uint64_t slide;
    std::filesystem::path path;
    uint8_t uuid[16];
    std::vector<uint8_t> bytes;
    sha256_t digest;
    bool is_jit;
    auto operator<=>(const image_info &rhs) const {
        return base <=> rhs.base;
    }
    std::filesystem::path log_path() const;
};

class XNUTRACE_EXPORT MachORegions {
public:
    MachORegions(task_t target_task);
    MachORegions(const log_region *region_buf, uint64_t num_regions,
                 std::map<sha256_t, std::vector<uint8_t>> &regions_bytes);
    void reset();
    const std::vector<image_info> &regions() const;
    XNUTRACE_INLINE const image_info &lookup(uint64_t addr) const;
    XNUTRACE_INLINE std::pair<const image_info &, size_t> lookup_idx(uint64_t addr) const;
    XNUTRACE_INLINE uint32_t lookup_inst(uint64_t addr) const;
    const image_info &lookup(const std::string &image_name) const;
    void dump() const;

private:
    struct region_lookup {
        uint64_t base;
        const uint8_t *buf;
        auto operator<=>(const region_lookup &rhs) const {
            return base <=> rhs.base;
        }
    };
    typedef pthash::single_phf<pthash::xxhash_64,             // base hasher
                               pthash::dictionary_dictionary, // encoder type
                               true                           // minimal
                               >
        pthash_type;
    void create_hash();
    const task_t m_target_task{};
    std::vector<image_info> m_regions;
    std::vector<const uint8_t *> m_regions_bufs;
    pthash_type m_page_addr_hasher;
};
