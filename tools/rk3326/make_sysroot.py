#!/usr/bin/env python3
"""Ubuntu 24.04 (noble, glibc 2.39) arm64 sysroot for the RK3326 cross build.

glibc 2.39 is what jbcgames' working ttyd-eu already requires, so the device has
at least that. libstdc++ is linked dynamically and the newer libstdc++.so.6 from
jbcgames' package ships beside the binary, as before.

usage: make_sysroot.py OUT_DIR
"""
import lzma
import os
import subprocess
import sys
import urllib.request

MIRROR = "http://ports.ubuntu.com/ubuntu-ports"
SUITES = ["noble", "noble-updates"]  # later suites override earlier ones
COMPONENTS = ["main", "universe"]
PACKAGES = [
    # C library, kernel headers
    "libc6", "libc6-dev", "linux-libc-dev", "libcrypt1", "libcrypt-dev",
    # GCC 14 runtime + C++ library headers (clang uses them)
    "libgcc-s1", "libgcc-14-dev", "libstdc++6", "libstdc++-14-dev",
    # GLES/EGL headers and the libGLESv2.so link target (libmali provides it on device)
    "libgles-dev", "libgles2", "libgles1", "libegl-dev", "libegl1", "libglvnd0", "libglvnd-dev", "libgl-dev",
    "libgl1", "libglx-dev", "libglx0", "libopengl-dev", "libopengl0",
    # zlib (ttyd-eu NEEDs libz.so.1, which the device has)
    "zlib1g", "zlib1g-dev",
]


def fetch(url):
    for attempt in range(4):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={"User-Agent": "sysroot-fetch/1.0"}), timeout=120) as r:
                return r.read()
        except OSError as e:
            if attempt == 3:
                sys.exit(f"make_sysroot: {url}: {e}")


def index():
    out = {}
    for suite in SUITES:
        for comp in COMPONENTS:
            text = lzma.decompress(fetch(f"{MIRROR}/dists/{suite}/{comp}/binary-arm64/Packages.xz")).decode()
            for stanza in text.split("\n\n"):
                f = {}
                for line in stanza.splitlines():
                    if line and not line[0].isspace() and ":" in line:
                        k, v = line.split(":", 1)
                        f[k] = v.strip()
                if "Package" in f and "Filename" in f:
                    out[f["Package"]] = (f["Filename"], f.get("Version", "?"))
    return out


def main():
    out = os.path.abspath(sys.argv[1])
    debs = os.path.join(out, "..", "debs")
    os.makedirs(out, exist_ok=True)
    os.makedirs(debs, exist_ok=True)
    idx = index()
    missing = [p for p in PACKAGES if p not in idx]
    if missing:
        sys.exit(f"make_sysroot: not in the index: {missing}")
    for p in PACKAGES:
        filename, version = idx[p]
        deb = os.path.join(debs, os.path.basename(filename))
        if not os.path.exists(deb):
            with open(deb, "wb") as f:
                f.write(fetch(f"{MIRROR}/{filename}"))
        subprocess.run(["dpkg-deb", "-x", deb, out], check=True)
        print(f"{p:20} {version}")
    # absolute symlinks (e.g. usr/lib/aarch64-linux-gnu/libz.so -> /lib/...) must resolve inside
    # the sysroot: make them relative
    for root, _, files in os.walk(out):
        for name in files:
            path = os.path.join(root, name)
            if os.path.islink(path):
                target = os.readlink(path)
                if target.startswith("/"):
                    os.remove(path)
                    os.symlink(os.path.relpath(out + target, root), path)
    # Ubuntu's merged /usr: /lib -> usr/lib
    for d in ("lib", "bin", "sbin"):
        link = os.path.join(out, d)
        if not os.path.exists(link):
            os.symlink(f"usr/{d}", link)
    print(f"sysroot ready: {out}")


main()
