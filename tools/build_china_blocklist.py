#!/usr/bin/env python3
"""One-click build of the China-enabled ESP32-C3 adblock blocklist.

Downloads (StevenBlack base + Hagezi Light + anti-AD), runs build_blocklist.py and
writes data/blocklist.bin. Optionally flashes it to the device via PlatformIO.

Robust on CN networks: each source tries raw.githubusercontent.com first, then the
jsDelivr CDN mirror, with retries.

Usage:
  python tools/build_china_blocklist.py                # build only
  python tools/build_china_blocklist.py --upload       # build + pio run -t uploadfs
"""
import os
import subprocess
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache")
os.makedirs(CACHE, exist_ok=True)

SOURCES = [
    ("stevenblack.hosts", [
        "https://raw.githubusercontent.com/StevenBlack/hosts/master/hosts",
        "https://cdn.jsdelivr.net/gh/StevenBlack/hosts@master/hosts",
    ]),
    ("hagezi_light.txt", [
        "https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/light-onlydomains.txt",
        "https://cdn.jsdelivr.net/gh/hagezi/dns-blocklists@main/wildcard/light-onlydomains.txt",
    ]),
    ("antiad.txt", [
        "https://raw.githubusercontent.com/privacy-protection-tools/anti-AD/master/anti-ad-domains.txt",
        "https://cdn.jsdelivr.net/gh/privacy-protection-tools/anti-AD@master/anti-ad-domains.txt",
    ]),
]

def download(name, urls):
    dest = os.path.join(CACHE, name)
    for url in urls:
        for attempt in range(1, 4):
            try:
                print(f"  {url} (try {attempt})")
                req = urllib.request.Request(url, headers={"User-Agent": "curl/8"})
                data = urllib.request.urlopen(req, timeout=120).read()
                if len(data) > 1000:
                    with open(dest, "wb") as f:
                        f.write(data)
                    print(f"  -> OK ({len(data):,} bytes)")
                    return True
            except Exception as e:
                print(f"  !! {e}")
    return False

def main():
    upload = "--upload" in sys.argv

    print("== 1/3 downloading blocklist sources ==")
    paths = []
    for name, urls in SOURCES:
        if not download(name, urls):
            sys.exit(f"Failed to download {name}. Check network. Aborting.")
        paths.append(os.path.join(CACHE, name))

    print("\n== 2/3 building data/blocklist.bin ==")
    cmd = [sys.executable, os.path.join("tools", "build_blocklist.py"),
           os.path.join("data", "blocklist.bin"), *paths]
    rc = subprocess.call(cmd, cwd=ROOT)
    if rc != 0:
        sys.exit(f"build_blocklist.py failed (exit {rc})")

    size = os.path.getsize(os.path.join(ROOT, "data", "blocklist.bin"))
    print(f"\nBlocklist: {size/1024:.1f} KB ({size/1024/1024:.2f} MB)")

    if upload:
        print("\n== 3/3 flashing filesystem (uploadfs) ==")
        rc = subprocess.call(["pio", "run", "-t", "uploadfs"], cwd=ROOT)
        if rc != 0:
            sys.exit(f"uploadfs failed (exit {rc})")
        print("Flash done.")

    print("\nDone.")

if __name__ == "__main__":
    main()
