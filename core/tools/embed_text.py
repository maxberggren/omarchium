#!/usr/bin/env python3
"""Embeds a text file into a C++ header as `inline constexpr char NAME[]`."""
import sys

src, out, name = sys.argv[1:4]
text = open(src, encoding="utf-8").read()
delim = "omarchium_embed"
assert f"){delim}\"" not in text
with open(out, "w", encoding="utf-8") as f:
    f.write(f"// Generated from {src.split('omarchium/')[-1]} by embed_text.py. Do not edit.\n")
    f.write("#pragma once\n\nnamespace omarchium {\n")
    f.write(f'inline constexpr char {name}[] = R"{delim}({text}){delim}";\n')
    f.write("}  // namespace omarchium\n")
