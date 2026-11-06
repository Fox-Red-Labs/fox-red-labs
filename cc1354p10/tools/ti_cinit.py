#!/usr/bin/env python3
"""Read and rewrite TI Arm linker copy-table (cinit) records in a flash image, without the ELF.
decompress_lzss() is a line-for-line port of TI's lib/src/copy_decompress_lzss.c (TI Arm Clang 5.1.1).
encode_literal() writes a valid LZSS stream made only of literal bytes plus the end marker, which
TI's own decompressor reads unchanged: no compressor needed."""
import struct

def decompress_lzss(buf, pos):
    out = bytearray()
    while True:
        flags = buf[pos]; pos += 1
        for _ in range(8):
            if flags & 1:
                out.append(buf[pos]); pos += 1
            else:
                offset = buf[pos]; length = buf[pos + 1]; pos += 2
                offset = (offset << 4) | ((length & 0xF0) >> 4)
                length = (length & 0x0F) + 3
                if length == 18:
                    l2 = buf[pos]; pos += 1
                    if l2 & 0x80:
                        l3 = buf[pos]; pos += 1
                        l2 = (l2 & 0x7F) | (l3 << 7)
                    length += l2
                if offset == 4095:
                    return bytes(out), pos
                src = len(out) - offset - 1
                for j in range(length):
                    out.append(out[src + j])
            flags >>= 1

def encode_literal(data):
    """LZSS stream of literals only, then the end-of-data reference (offset 4095)."""
    out = bytearray(); i = 0
    while i < len(data):
        chunk = data[i:i + 8]; i += 8
        if len(chunk) == 8:
            out.append(0xFF); out += chunk
        else:
            # remaining literals, then one reference slot for the end marker
            out.append((1 << len(chunk)) - 1); out += chunk; out += bytes([0xFF, 0xF0])
            return bytes(out)
    out.append(0x00); out += bytes([0xFF, 0xF0])
    return bytes(out)

def records(img, table, count):
    return [struct.unpack_from('<II', img, table + 8 * k) for k in range(count)]
