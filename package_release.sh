#!/usr/bin/env bash
set -eu

# Create a release package with static executable and documentation
mkdir -p release
rm -rf release/*

# Copy the static executable
cp bin/pulse_gen_static release/pulse_gen || true

# Create a wrapper script that checks for ffmpeg
cat > release/pulse_gen.sh << 'EOF'
#!/usr/bin/env bash
# Check if ffmpeg is available
if ! command -v ffmpeg &> /dev/null; then
    echo "Error: ffmpeg is required but not found."
    echo "Please install ffmpeg (e.g., 'sudo apt install ffmpeg' or equivalent)"
    exit 1
fi

# Pass arguments to the main executable
exec "$(dirname "$0")/pulse_gen" "$@"
EOF

chmod +x release/pulse_gen.sh

# Create README for the release
cat > release/README.txt << 'EOF'
PULSE VIDEO GENERATOR - Static Release

This package contains:
- pulse_gen: The static executable (renamed from pulse_gen_static)
- pulse_gen.sh: A wrapper script that checks for ffmpeg dependency
- This README file

DEPENDENCY:
This tool requires ffmpeg to be installed on your system to encode the video.
On most Linux distributions, you can install it with:
- Ubuntu/Debian: sudo apt install ffmpeg
- Fedora: sudo dnf install ffmpeg
- Arch: sudo pacman -S ffmpeg
- Alpine: sudo apk add ffmpeg

USAGE:
1. Make the files executable: chmod +x pulse_gen pulse_gen.sh
2. Run with: ./pulse_gen.sh --bpm 120 --out output.mp4 [other options]

EXAMPLE:
./pulse_gen.sh --bpm 120 --out output.mp4 --duration 30 --width 1080 --height 1920

Note: The core executable is static and contains no shared library dependencies,
but it uses popen() to launch ffmpeg for video encoding.
EOF

# Make the static executable in the release folder executable
chmod +x release/pulse_gen

echo "Release package created in ./release/ directory"
echo "Contents:"
ls -la release/