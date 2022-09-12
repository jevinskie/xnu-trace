import struct
import timeit
from pathlib import Path

import numba
import numpy as np
import numpy.typing as npt
from attrs import define
from compressedfile import CompressedFile

# struct log_meta_hdr {
#     uint64_t num_regions;
#     uint64_t num_syms;
# }
log_meta_hdr_t = struct.Struct("=QQ")
log_meta_hdr_magic = 0x8D3A_DFB8_4154_454D

# struct log_region {
#     uint64_t base;
#     uint64_t size;
#     uint64_t slide;
#     uuid_t uuid;
#     uint64_t path_len;
# }
log_region_t = struct.Struct("=QQQ16BQ")

# struct log_sym {
#     uint64_t base;
#     uint64_t size;
#     uint64_t name_len;
#     uint64_t path_len;
# }
log_sym_t = struct.Struct("=QQQQ")

# struct log_thread_hdr {
#     uint64_t thread_id;
# }
log_thread_hdr_t = struct.Struct("=Q")
log_thread_hdr_magic = 0x8D3A_DFB8_4452_4854

# struct log_msg_hdr {
#     uint64_t pc;
# }
log_msg_hdr_t = struct.Struct("=Q")


@define
class MachORegion:
    base: int
    size: int
    slide: int
    uuid: bytes
    path: Path


@define
class Symbol:
    base: int
    size: int
    name: str
    path: str


class TraceLog:
    def __init__(self, trace_dir: str) -> None:
        self.macho_regions, self.syms, self.all_pcs = self._parse(trace_dir)

    @staticmethod
    def _parse(trace_dir: str) -> tuple[list[MachORegion], list[Symbol], npt.NDArray[np.uint64]]:
        trace_dir = Path(trace_dir)
        meta_fh = CompressedFile(trace_dir / "meta.bin", log_meta_hdr_magic, log_meta_hdr_t.size)
        meta_buf = bytes(meta_fh)

        num_regions, num_syms = log_meta_hdr_t.unpack(meta_fh.header())

        macho_regions = []
        region_buf_off = 0
        for i in range(num_regions):
            region_unpacked = log_region_t.unpack_from(meta_buf, offset=region_buf_off)
            base = region_unpacked[0]
            sz = region_unpacked[1]
            slide = region_unpacked[2]
            uuid = bytes(region_unpacked[3:-1])
            path_len = region_unpacked[-1]
            path_start = region_buf_off + log_region_t.size
            path_end = path_start + path_len
            path = meta_buf[path_start:path_end].decode("utf-8")
            macho_regions.append(MachORegion(base, sz, slide, uuid, Path(path)))
            region_buf_off += log_region_t.size + path_len

        syms = []
        sym_buf_off = region_buf_off
        for i in range(num_syms):
            base, sz, name_len, path_len = log_sym_t.unpack_from(meta_buf, offset=sym_buf_off)
            name_start = sym_buf_off + log_sym_t.size
            name_end = name_start + name_len
            name = meta_buf[name_start:name_end].decode("utf-8")
            path_start = name_end
            path_end = path_start + path_len
            path = meta_buf[path_start:path_end].decode("utf-8")
            syms.append(Symbol(base, sz, name, path))
            sym_buf_off += log_sym_t.size + name_len + path_len

        traces = {}

        num_all_pcs = 0
        for trace_path in trace_dir.iterdir():
            if trace_path.name == "meta.bin":
                continue
            assert trace_path.name.startswith("thread-")
            trace_fh = CompressedFile(trace_path, log_thread_hdr_magic, log_thread_hdr_t.size)
            trace_buf = bytes(trace_fh)
            (thread_id,) = log_thread_hdr_t.unpack(trace_fh.header())
            num_pcs = len(trace_buf) // 8
            ndarray = np.ndarray(num_pcs, dtype=np.uint64, buffer=trace_buf)
            traces[thread_id] = ndarray
            num_all_pcs += num_pcs

        all_pcs = np.ndarray(num_all_pcs, dtype=np.uint64)
        num_pcs = 0
        for tpcs in traces.values():
            all_pcs[num_pcs : num_pcs + len(tpcs)] = tpcs
            num_pcs += len(tpcs)

        return macho_regions, syms, all_pcs

    def dump(self) -> None:
        for i, r in enumerate(self.macho_regions):
            print(f"region[{i:3d}]: {r}")
        for i, s in enumerate(self.syms):
            print(f"sym[{i:3d}]: {s}")

    def lookup_image(self, img_name: str) -> tuple[int, int]:
        for r in self.macho_regions:
            if r.path.name == img_name:
                return r.base, r.size
        else:
            raise ValueError(f"can't find {img_name} in macho_regions")

    @staticmethod
    @numba.njit(parallel=True)
    def pcs_in_range1(
        all_pcs: npt.NDArray[np.uint64], min: int, max: int
    ) -> npt.NDArray[np.uint64]:
        return all_pcs[(all_pcs[:] >= min) & (all_pcs[:] < max)]

    @staticmethod
    @numba.njit
    def pcs_in_range2(
        all_pcs: npt.NDArray[np.uint64], min: int, max: int
    ) -> npt.NDArray[np.uint64]:
        pcs = np.zeros_like(all_pcs)
        i = 0
        for pc in all_pcs:
            if min <= pc < max:
                pcs[i] = pc
                i += 1
        return pcs[:i]

    @staticmethod
    @numba.njit(parallel=True)
    def based_pcs_in_range(
        all_pcs: npt.NDArray[np.uint64], min: int, max: int
    ) -> npt.NDArray[np.uint32]:
        return all_pcs[(all_pcs[:] >= min) & (all_pcs[:] < max)] - min

    def pcs_for_image(self, img_name: str) -> npt.NDArray[np.uint64]:
        base, size = self.lookup_image(img_name)
        return self.pcs_in_range1(self.all_pcs, base, base + size)

    def based_pcs_for_image(self, img_name: str) -> npt.NDArray[np.uint32]:
        base, size = self.lookup_image(img_name)
        return self.based_pcs_in_range(self.all_pcs, base, base + size)
