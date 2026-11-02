#!/usr/bin/env python3
"""
Deterministic generator for 3DS Drop Server (3ds_drop) packaging assets:
- meta/icon.png (48x48 RGBA PNG: Electric Cyan/Cobalt SD Card + Bidirectional Wireless Arrow)
- meta/banner.png (256x128 RGBA PNG: Wireless Cloud/SD Transfer emblem)
- meta/audio.wav & meta/banner.bnr
"""

import math
import os
import struct
import wave
import zlib
from generate_bnr_shared import draw_banner_text_centered, write_cbmd_bnr  # type: ignore


def write_png(path: str, width: int, height: int, rgba_bytes: bytes) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    raw_rows = bytearray()
    stride = width * 4
    for y in range(height):
        raw_rows.append(0)
        raw_rows.extend(rgba_bytes[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    compressed = zlib.compress(bytes(raw_rows), 9)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", compressed))
        f.write(chunk(b"IEND", b""))


def generate_icon(path: str) -> None:
    w, h = 48, 48
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            # Electric cyan border
            if x < 2 or x >= w - 2 or y < 2 or y >= h - 2:
                buf[idx : idx + 4] = bytes((65, 205, 255, 255))
                continue
            buf[idx : idx + 4] = bytes((12, 18, 36, 255))

            # MicroSD card silhouette (x: 11..36, y: 7..40) with top-right notch cut out
            in_sd = (11 <= x <= 36) and (7 <= y <= 40) and not (x > 29 and y < 14 and (x - 29) > (y - 7))
            if in_sd:
                buf[idx : idx + 4] = bytes((28, 52, 96, 255))
                # Gold SD contact pads at top (y: 10..15)
                if 10 <= y <= 15 and x in (14, 15, 18, 19, 22, 23, 26, 27):
                    buf[idx : idx + 4] = bytes((250, 210, 80, 255))
                    continue

                # Bright Cyan Down-Arrow / Upload-Download emblem in center of SD card
                # Shaft: x 21..26, y 18..29; Arrowhead: y 28..35, |x - 23.5| <= (35 - y)
                if (21 <= x <= 26 and 18 <= y <= 28) or (
                    28 <= y <= 35 and abs(x - 23.5) <= (35 - y)
                ):
                    buf[idx : idx + 4] = bytes((80, 240, 255, 255))

    write_png(path, w, h, bytes(buf))


def generate_banner(path: str) -> bytes:
    w, h = 256, 128
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 4
            r = 12 + (y * 14) // h
            g = 24 + (y * 28) // h
            b = 48 + (y * 42) // h
            buf[idx : idx + 4] = bytes((r, g, b, 255))

            if 100 <= x <= 156 and 14 <= y <= 76:
                if x in (100, 156) or y in (14, 76):
                    buf[idx : idx + 4] = bytes((75, 215, 255, 255))
                elif (122 <= x <= 134 and 26 <= y <= 54) or (
                    54 <= y <= 70 and abs(x - 128) <= (70 - y)
                ):
                    buf[idx : idx + 4] = bytes((95, 245, 255, 255))

    draw_banner_text_centered(buf, w, h, 86, "3DS DROP", 2, (105, 240, 255, 255))
    draw_banner_text_centered(buf, w, h, 108, "WIRELESS SD FILE SERVER", 1, (220, 240, 255, 255))
    raw = bytes(buf)
    write_png(path, w, h, raw)
    return raw


def generate_audio(path: str) -> None:
    sample_rate = 22050
    duration_s = 0.8
    n_samples = int(sample_rate * duration_s)
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        frames = bytearray()
        for i in range(n_samples):
            t = i / sample_rate
            freq = 659.25 if t < 0.25 else (783.99 if t < 0.5 else 1046.50)
            env = max(0.0, 1.0 - (t / duration_s))
            val = int(12000 * env * math.sin(2.0 * math.pi * freq * t))
            frames.extend(struct.pack("<h", val))
        wf.writeframes(bytes(frames))


def main() -> None:
    meta_dir = os.path.dirname(os.path.abspath(__file__))
    generate_icon(os.path.join(meta_dir, "icon.png"))
    banner_rgba = generate_banner(os.path.join(meta_dir, "banner.png"))
    generate_audio(os.path.join(meta_dir, "audio.wav"))
    write_cbmd_bnr(os.path.join(meta_dir, "banner.bnr"), (659.25, 783.99, 1046.50), banner_rgba)
    print("Generated 3DS Drop meta/icon.png, meta/banner.png, meta/audio.wav, meta/banner.bnr")


if __name__ == "__main__":
    main()
