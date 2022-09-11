import struct


class CompressedFile:
    def __init__(self, path: str) -> None:
        self.fh = open(path, "rb")
