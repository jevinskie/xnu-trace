import locale
import mmap
import struct

import zstandard

locale.setlocale(locale.LC_ALL, "")

# struct log_comp_hdr {
#     uint64_t magic;
#     uint64_t is_compressed;
#     uint64_t header_size;
#     uint64_t decompressed_size;
# }
log_comp_hdr_t = struct.Struct("=QQQQ")


class CompressedFile:
    def __init__(self, path: str, magic: int, hdr_size: int) -> None:
        self.path = path
        self.fh = open(path, "rb")
        comp_hdr_buf = self.fh.read(log_comp_hdr_t.size)
        (self.magic, is_comp, self.header_size, self.decompressed_size) = log_comp_hdr_t.unpack(
            comp_hdr_buf
        )
        self.is_compressed = bool(is_comp)
        assert self.magic == magic
        assert self.header_size == hdr_size
        self.header_buf = self.fh.read(self.header_size)
        self.data_off = log_comp_hdr_t.size + self.header_size
        if self.is_compressed:
            self.decomp_mmap = mmap.mmap(-1, self.decompressed_size)
            decompressor = zstandard.ZstdDecompressor()
            self.fh.seek(self.data_off)
            (num_bytes_read, num_bytes_written) = decompressor.copy_stream(
                self.fh, self.decomp_mmap
            )
            print(f"'{path}' decompressed {num_bytes_read:n} bytes to {num_bytes_written:n} bytes")
        else:
            self.decomp_mmap = self.fh.read(self.decompressed_size)

    def __bytes__(self) -> mmap.mmap:
        return bytes(self.decomp_mmap)

    def header(self) -> bytes:
        return self.header_buf
