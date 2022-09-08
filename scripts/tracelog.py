import struct
from pathlib import Path

from attrs import define

log_hdr_t = struct.Struct("=QQQ")
log_region_t = struct.Struct("=QQQ16BQ")
log_sym_t = struct.Struct("=QQQQ")
log_thread_hdr_t = struct.Struct("=IQ")
log_msg_hdr_t = struct.Struct("=Q")


@define
class MachORegion:
    base: int
    size: int
    slide: int
    uuid: bytes
    path: str


@define
class Symbol:
    base: int
    size: int
    name: str
    path: str


class TraceLog:
    def __init__(self, trace_path: str) -> None:
        self.macho_regions, self.syms, self.traces = self._parse(trace_path)

    def _parse(
        self, trace_path: str
    ) -> tuple[list[MachORegion], list[Symbol], dict[int, tuple[int]]]:
        tl_buf = open(trace_path, "rb").read()
        tl_buf_sz = len(tl_buf)

        magic, num_regions, num_syms = log_hdr_t.unpack_from(tl_buf, offset=0)

        assert magic == 0x8D3A_DFB8_A584_33F9

        macho_regions = []
        region_buf_off = log_hdr_t.size
        for i in range(num_regions):
            region_unpacked = log_region_t.unpack_from(tl_buf, offset=region_buf_off)
            base = region_unpacked[0]
            sz = region_unpacked[1]
            slide = region_unpacked[2]
            uuid = bytes(region_unpacked[3:-1])
            path_len = region_unpacked[-1]
            path_start = region_buf_off + log_region_t.size
            path_end = path_start + path_len
            path = tl_buf[path_start:path_end].decode("utf-8")
            macho_regions.append(MachORegion(base, sz, slide, uuid, path))
            region_buf_off += log_region_t.size + path_len

        syms = []
        sym_buf_off = region_buf_off
        for i in range(num_syms):
            base, sz, name_len, path_len = log_sym_t.unpack_from(tl_buf, offset=sym_buf_off)
            name_start = sym_buf_off + log_sym_t.size
            name_end = name_start + name_len
            name = tl_buf[name_start:name_end].decode("utf-8")
            path_start = name_end
            path_end = path_start + path_len
            path = tl_buf[path_start:path_end].decode("utf-8")
            syms.append(Symbol(base, sz, name, path))
            sym_buf_off += log_sym_t.size + name_len + path_len

        traces = {}
        thread_hdr_off = sym_buf_off
        print(f"thread_hdr_off: {thread_hdr_off:#x}")
        while thread_hdr_off < tl_buf_sz:
            thread_id, thread_log_sz = log_thread_hdr_t.unpack_from(tl_buf, offset=thread_hdr_off)
            num_inst = thread_log_sz // log_msg_hdr_t.size
            print(f"!!! tid: {thread_id} log_sz: {thread_log_sz} num_inst: {num_inst}")
            pc_log_off = thread_hdr_off + log_thread_hdr_t.size
            pc_log = struct.unpack_from(f"{num_inst}Q", tl_buf, offset=pc_log_off)
            traces[thread_id] = pc_log
            thread_hdr_off += log_thread_hdr_t.size + thread_log_sz

        return macho_regions, syms, traces

    def dump(self) -> None:
        for i, r in enumerate(self.macho_regions):
            print(f"region[{i:3d}]: {r}")
        for i, s in enumerate(self.syms):
            print(f"sym[{i:3d}]: {s}")
        for thread_id, pc_log in self.traces.items():
            print(f"thread {thread_id:d}")
            for pc in pc_log:
                print(f"{pc:#018x}")

    def pcs_for_image(self, img_name: str) -> list[int]:
        for r in self.macho_regions:
            if Path(r.path).name == img_name:
                start = r.base
                end = r.base + r.size
                break
        else:
            raise ValueError(f"can't find {img_name} in macho_regions")
        pcs = []
        for tpcs in self.traces.values():
            pcs += [pc for pc in tpcs if start <= pc < end]
        return pcs
