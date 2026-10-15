#!/bin/bash
set -e
# Builds the Linux plugin package into build/package (Steam Runtime "sniper" SDK image, same as CS2KZ).
cd build
python3 ../configure.py --enable-optimize
ambuild || exit 1
