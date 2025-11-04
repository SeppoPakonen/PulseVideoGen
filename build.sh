!/bin/sh
set -eu
mkdir -p build bin
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release -j
cd ..
cp build/pulse_gen bin/pulse_gen || true
echo "Built: bin/pulse_gen"
echo "Run: bin/pulse_gen --bpm 120 --out output.mp4"

