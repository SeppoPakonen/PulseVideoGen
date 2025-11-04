#!/usr/bin/env bash
set -eu
mkdir -p build_static bin
cd build_static
cmake -DCMAKE_BUILD_TYPE=Release -DSTATIC_LINK=ON ..
cmake --build . --config Release -j
cd ..
cp build_static/pulse_gen bin/pulse_gen_static || true
echo "Built: bin/pulse_gen_static"
echo "Run: bin/pulse_gen_static --bpm 120 --out output.mp4"