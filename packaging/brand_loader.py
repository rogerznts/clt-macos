"""Rename the app shown in the macOS menu bar from "Wine" to CLT.

AppKit takes the menu-bar name from the Info.plist embedded in the Wine loader's __TEXT,__info_plist section
(CFBundleName "Wine"). The plist is rewritten in place with the same byte length, then the binary is re-signed.

usage: brand_loader.py <wine loader> <name>
"""
import re
import subprocess
import sys


def main(path, name):
    data = open(path, "rb").read()
    start = data.find(b"<?xml version")
    end = data.find(b"</plist>", start) + len(b"</plist>")
    if start < 0 or end < len(b"</plist>"):
        sys.exit("no embedded Info.plist in " + path)
    plist = data[start:end]
    branded = re.sub(rb"(<key>CFBundleName</key>\s*<string>)[^<]*(</string>)",
                     lambda m: m.group(1) + name.encode() + m.group(2), plist)
    branded = re.sub(rb"(<key>CFBundleIdentifier</key>\s*<string>)[^<]*(</string>)",
                     lambda m: m.group(1) + b"local.port.clt" + m.group(2), branded)
    if len(branded) > len(plist):
        sys.exit("branded plist is longer than the original section")
    # Pad with whitespace between </dict> and </plist>, where XML ignores it, to keep the section size.
    branded = branded.replace(b"</plist>", b" " * (len(plist) - len(branded)) + b"</plist>")
    open(path, "wb").write(data[:start] + branded + data[end:])
    subprocess.run(["codesign", "-f", "-s", "-", path], check=True, capture_output=True)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
