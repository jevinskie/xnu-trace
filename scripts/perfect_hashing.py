#!/usr/bin/env python3

import hashlib
import sys
from os import dup

import xxhash
from iteration_utilities import duplicates, unique_everseen


class MPH:
    def __init__(self, keys: list[int]) -> None:
        self.keys = keys
        hashes = {k: self.hash(k) for k in keys}
        buckets = {}
        self.nkeys = nkeys = len(hashes)
        for k, h in hashes.items():
            hmod = h % nkeys
            if hmod not in buckets:
                buckets[hmod] = [k]
            else:
                buckets[hmod].append(k)
        sorted_buckets = {
            h: ks for h, ks in sorted(buckets.items(), key=lambda i: len(i[1]), reverse=True)
        }
        print(list(sorted_buckets.items())[0])
        lsb = list(sorted_buckets.values())

        svs = [0] * nkeys

        for i, hb in enumerate(sorted_buckets.items()):
            h, bucket = hb
            if len(bucket) > 1:
                d = 1
                while True:
                    sub_hashes = [self.hash(k, d) % nkeys for k in bucket]
                    all_free = all([svs[sh] == 0 for sh in sub_hashes])
                    if all_free:
                        svs[h] = d
                        # print(f"i: {i} d: {d}")
                        break
                    if d > 1024:
                        raise RuntimeError("too long")
                    d += 1
            else:
                free_idx = svs.index(0)
                svs[h] = -free_idx - 1
                pass

        self.svs = svs

    @staticmethod
    def hash(v: int, d: int = 0) -> int:
        return xxhash.xxh64_intdigest(v.to_bytes(8, "little"), d)

    def lookup_idx(self, k: int) -> int:
        sv = self.svs[self.hash(k) % self.nkeys]
        if sv < 0:
            return -sv - 1
        else:
            return self.hash(k, sv) % self.nkeys

    def check(self):
        print(f"self.svs.count(0): {self.svs.count(0)}")
        assert self.svs.count(0) == 1
        idxes = [self.lookup_idx(k) for k in self.keys]
        print(f"len(self.keys): {len(self.keys)}")
        print(f"len(set(idxes)): {len(set(idxes))}")
        assert (
            min(idxes) == 0
            and max(idxes) == (len(self.keys) - 1)
            and len(set(idxes)) == len(self.keys)
        )


page_addrs = []

for line in open(sys.argv[1]).readlines():
    page_addrs.append(int(line.split()[1], 16))

page_addrs = list(set(page_addrs))
print(f"len(page_addrs): {len(page_addrs)}")

mph = MPH(page_addrs)
mph.check()

# dup_pas = [pa for pa in page_addrs if page_addrs.count(pa) > 1]
# for pa in dup_pas:
#     print(f"{pa:#018x}")

# print(len(page_addrs))
# print(len(set(page_addrs)))

# hashes = []
# for pa in page_addrs:
#     h = xxhash.xxh64_intdigest(pa.to_bytes(8, "little"))
#     hashes.append(h)

# hashes2 = []
# for pa in page_addrs:
#     h = xxhash.xxh64_intdigest(pa.to_bytes(8, "little"), 0x23E8FCE423929859)
#     hashes2.append(h)

# hashes3 = []
# for pa in page_addrs:
#     h = xxhash.xxh64_intdigest(pa.to_bytes(8, "little"), 0x4CA43C7C57D6E1AC)
#     hashes3.append(h)

# hashes4 = []
# for pa in page_addrs:
#     h = xxhash.xxh64_intdigest(pa.to_bytes(8, "little"), 0x88563B2CF574E78B)
#     hashes4.append(h)


# print(f"len(set(hashes)): {len(set(hashes))}")
# print(f"len(set(hashes2)): {len(set(hashes2))}")
# print(f"len(set(hashes3)): {len(set(hashes3))}")
# print(f"len(set(hashes4)): {len(set(hashes4))}")


# dups_hash = list(unique_everseen(duplicates([h % len(page_addrs) for h in hashes])))
# dups_hash2 = list(unique_everseen(duplicates([h % len(page_addrs) for h in hashes2])))
# dups_hash3 = list(unique_everseen(duplicates([h % len(page_addrs) for h in hashes3])))
# dups_hash4 = list(unique_everseen(duplicates([h % len(page_addrs) for h in hashes4])))

# print(f"len(dups_hash): {len(dups_hash)}")
# print(f"len(dups_hash2): {len(dups_hash2)}")
# print(f"len(dups_hash3): {len(dups_hash3)}")
# print(f"len(dups_hash4): {len(dups_hash4)}")

# print("dups_hash:")
# print("\n".join([f"{h:#018x}" for h in dups_hash]))
# print("\n" * 10)
# print("dups_hash2:")
# print("\n".join([f"{h:#018x}" for h in dups_hash2]))

# same = list(set(dups_hash).intersection(set(dups_hash2)))
# print(f"dups_hash & dups_hash2 sz {len(same)}")
# print("\n".join([f"{h:#018x}" for h in same]))

# print("\n" * 10)

# same2 = list(set(dups_hash).intersection(set(dups_hash2).intersection(set(dups_hash3))))
# print(f"dups_hash & dups_hash2 & dups_hash3 sz {len(same2)}")
# print("\n".join([f"{h:#018x}" for h in same2]))

# same3 = list(
#     set(dups_hash).intersection(
#         set(dups_hash2).intersection(set(dups_hash3).intersection(dups_hash4))
#     )
# )
# print(f"dups_hash & dups_hash2 & dups_hash3 & dups_hash4 sz {len(same3)}")
# print("\n".join([f"{h:#018x}" for h in same3]))

# sha_hashes = []
# for pa in page_addrs:
#     h = int.from_bytes(hashlib.sha256(pa.to_bytes(8, "little")).digest()[:16], "little")
#     sha_hashes.append(h % len(page_addrs))

# print(len(hashes))
# print(len(set(hashes)))
