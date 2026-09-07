#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Extract the set of CJK characters used in UI string literals.

Scans the given C/H source files, strips comments, parses double-quoted
string literals, and emits the de-duplicated, sorted set of CJK characters
(+ CJK punctuation / full-width forms) so it can be fed to lv_font_conv as
the --symbols argument.

Usage:
    python gen_chat_font.py [file1.c file2.h ...] [-o symbols.txt]
If no files are given, the default ESP32 main UI sources are scanned.
"""

import argparse
import os
import sys

DEFAULT_SRCS = [
    "ai_chat_ui.c",
    "lcd_ui.c",
    "wifi_prov_ui.c",
    "voice_config.c",
    "convai_platform_esp32.c",
]

CJK_RANGES = (
    (0x3000, 0x303F),   # CJK symbols and punctuation
    (0x4E00, 0x9FFF),   # CJK unified ideographs
    (0xFF00, 0xFFEF),   # full-width forms
)


def in_cjk(cp):
    return any(lo <= cp <= hi for lo, hi in CJK_RANGES)


def strip_comments(src):
    """Remove // and /* */ comments while respecting string/char literals."""
    out = []
    i, n = 0, len(src)
    state = "code"  # code, line_comment, block_comment, string, char
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == "/" and nxt == "/":
                state = "line_comment"; i += 2; continue
            if c == "/" and nxt == "*":
                state = "block_comment"; i += 2; continue
            if c == '"':
                state = "string"; out.append(c); i += 1; continue
            if c == "'":
                state = "char"; out.append(c); i += 1; continue
            out.append(c); i += 1
        elif state == "line_comment":
            if c == "\n":
                state = "code"; out.append(c)
            i += 1
        elif state == "block_comment":
            if c == "*" and nxt == "/":
                state = "code"; i += 2
            else:
                i += 1
        elif state == "string":
            out.append(c)
            if c == "\\" and nxt:
                out.append(nxt); i += 2; continue
            if c == '"':
                state = "code"
            i += 1
        elif state == "char":
            out.append(c)
            if c == "\\" and nxt:
                out.append(nxt); i += 2; continue
            if c == "'":
                state = "code"
            i += 1
    return "".join(out)


def extract_strings(src):
    """Yield contents of double-quoted string literals (escapes resolved)."""
    i, n = 0, len(src)
    while i < n:
        if src[i] == '"':
            i += 1
            buf = []
            while i < n:
                c = src[i]
                if c == "\\" and i + 1 < n:
                    buf.append(src[i + 1]); i += 2; continue
                if c == '"':
                    break
                buf.append(c); i += 1
            i += 1
            yield "".join(buf)
        else:
            i += 1


def read_text(path):
    raw = open(path, "rb").read()
    for enc in ("utf-8", "gbk"):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return raw.decode("utf-8", errors="replace")


def collect(files):
    chars = set()
    for f in files:
        if not os.path.exists(f):
            print(f"warn: {f} not found", file=sys.stderr)
            continue
        text = read_text(f)
        cleaned = strip_comments(text)
        for s in extract_strings(cleaned):
            for ch in s:
                if in_cjk(ord(ch)):
                    chars.add(ch)
    return chars


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="*")
    ap.add_argument("-o", "--output")
    ap.add_argument("--base", default=None,
                    help="base dir for default sources")
    args = ap.parse_args()

    if args.files:
        files = args.files
    else:
        base = args.base or os.path.join(
            os.path.dirname(__file__), "..", "main")
        files = [os.path.join(base, name) for name in DEFAULT_SRCS]

    chars = collect(files)
    symbols = "".join(sorted(chars))
    print(f"# {len(chars)} unique CJK chars from {len(files)} files",
          file=sys.stderr)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as fh:
            fh.write(symbols)
        print(f"# wrote symbols to {args.output}", file=sys.stderr)
    else:
        sys.stdout.write(symbols)


if __name__ == "__main__":
    main()
