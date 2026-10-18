#!/usr/bin/env python3
"""Compile the real backend with a synthetic verified image and read its stream."""
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
(root / "build").mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix="romfs-stream-paths-", dir=root / "build") as folder:
    work = Path(folder)
    name = b"Sound/Resource/Stream/Fixture.bwav\0"
    payload = bytes((i * 37 + i // 251 + 17) & 255 for i in range(40000))
    index = struct.pack("<8sQQ", b"TOTKRFS1", 1, 48) + struct.pack("<QQII", 0, len(payload), 0, len(name)) + name
    (work / "romfs.bin").write_bytes(payload)
    (work / "runtime-index.bin").write_bytes(index)
    (work / "romfs_config.h").write_text(
        f'#define ROMFS_IMAGE_SIZE {len(payload)}ULL\n'
        f'#define ROMFS_IMAGE_SHA256 "{hashlib.sha256(payload).hexdigest()}"\n'
        f'#define ROMFS_INDEX_SHA256 "{hashlib.sha256(index).hexdigest()}"\n'
    )
    subprocess.run([
        "xcrun", "clang", "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
        "-Wno-deprecated-declarations", f"-I{work}", f"-I{root / 'runtime'}",
        str(root / "runtime/romfs.c"), str(Path(__file__).with_name("probe.c")),
        "-o", str(work / "probe"),
    ], check=True)
    env = dict(os.environ, TOTK_ROMFS=str(work), TOTK_ROMFS_PATCHES="0", TOTK_ROMFS_PROOF_DIR=str(work))
    # Test both production I/O backends against the same independent bytes.
    for mmap in ("0", "1"):
        subprocess.run([str(work / "probe")], env=dict(env, TOTK_ROMFS_MMAP=mmap), check=True)
