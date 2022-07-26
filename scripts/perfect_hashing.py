#!/usr/bin/env python3

import random
import sys

import progressbar
import xxhash
from iteration_utilities import duplicates, unique_everseen


class MPH:
    def __init__(self, keys: list[int]) -> None:
        assert len(set(keys)) == len(keys)
        self.keys = keys = sorted(keys)
        self.nkeys = nkeys = len(keys)
        hashes = {k: self.hash(k) for k in keys}
        buckets = [(None, [])] * nkeys
        for k, h in hashes.items():
            hmod = h % nkeys
            buckets[hmod] = (hmod, buckets[hmod][1] + [k])
        # sort this way to match c++ impl (not matched anymore, but does provide a total ordering here)
        sorted_buckets = list(reversed(sorted(buckets, key=lambda t: (len(t[1]), t[0]))))
        # print(sorted_buckets[0])

        salts = [None] * nkeys
        slot_used = [False] * nkeys
        # for hash, bucket in sorted_buckets:
        for hash, bucket in progressbar.progressbar(sorted_buckets):
            if len(bucket) > 1:
                d = 1
                while True:
                    if d > 4096:
                        raise RuntimeError("taking too long, sorry")
                    salted_hashes = [self.hash(k, d) % nkeys for k in bucket]
                    if len(set(salted_hashes)) != len(salted_hashes):
                        # collision within salted hashes, try again
                        d += 1
                        continue
                    all_free = all([not slot_used[sh] for sh in salted_hashes])
                    if all_free:
                        for sh in salted_hashes:
                            slot_used[sh] = True
                        salts[hash] = d
                        break
                    d += 1
            elif len(bucket) == 1:
                free_idx = slot_used.index(False)
                slot_used[free_idx] = True
                salts[hash] = -free_idx - 1
        self.salts = salts

    @staticmethod
    def hash(v: int, d: int = 0) -> int:
        return xxhash.xxh64_intdigest(v.to_bytes(8, "little"), d)

    def lookup_idx(self, k: int) -> int:
        sv = self.salts[self.hash(k) % self.nkeys]
        if sv < 0:
            return -sv - 1
        else:
            return self.hash(k, sv) % self.nkeys

    def check(self):
        idxes = sorted([self.lookup_idx(k) for k in self.keys])
        assert idxes[0] == 0
        assert idxes[-1] == len(self.keys) - 1
        assert len(set(idxes)) == len(self.keys)
        assert len(self.salts) == len(self.keys)


page_addrs = []

for line in open(sys.argv[1]).readlines():
    page_addrs.append(int(line.split()[1], 16))

page_addrs = sorted(list(set(page_addrs)))

# with open("page_addrs.bin", "wb") as f:
#     for pa in page_addrs:
#         f.write(pa.to_bytes(8, "little"))

# print(f"len(page_addrs): {len(page_addrs)}")

mph = MPH(page_addrs)
mph.check()
print(f"max(mph.salts): {max([s for s in mph.salts if s is not None])}")
print(f"{mph.salts.count(None) / mph.nkeys * 100:0.2f}% filled")

# random.seed(243)
rand_u64 = [random.randint(0, 0xFFFF_FFFF_FFFF_FFFF) for i in range(100_000)]

# with open("rand_u64_dup_idx_29751.bin", "wb") as f:
#     for n in rand_u64:
#         f.write(n.to_bytes(8, "little"))

mph_rand = MPH(rand_u64)
mph_rand.check()
print(f"max(mph_rand.salts): {max([s for s in mph_rand.salts if s is not None])}")
print(f"{mph_rand.salts.count(None) / mph_rand.nkeys * 100:0.2f}% filled")
