#!/usr/bin/env bash
set -eu

if [ "$#" -eq 0 ]; then
    # Default build behavior
    mkdir -p build bin
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release -DVERSION="v1.0" ..
    cmake --build . --config Release -j
    cd ..
    cp build/pulse_gen bin/pulse_gen || true
    echo "Built: bin/pulse_gen"
    echo "Run: bin/pulse_gen --bpm 120 --out output.mp4"
elif [ "$1" = "release" ]; then
    # Create a release with version and platform
    if [ -z "${2:-}" ] || [ -z "${3:-}" ]; then
        echo "Usage: $0 release <version> <platform>"
        echo "Example: $0 release v0.0.1 linux64"
        echo "Supported platforms: linux64, linux32, linux-arm64, linux-arm32, win64, win32, win-arm64, user"
        exit 1
    fi
    
    VERSION="$2"
    PLATFORM="$3"
    
    # Validate platform
    VALID_PLATFORMS="linux64 linux32 linux-arm64 linux-arm32 win64 win32 win-arm64 user"
    if ! echo "$VALID_PLATFORMS" | grep -qw "$PLATFORM"; then
        echo "Error: Invalid platform '$PLATFORM'. Supported platforms: $VALID_PLATFORMS"
        exit 1
    fi
    
    echo "Creating release: $VERSION for platform: $PLATFORM"
    
    # Build the static executable with the correct version
    mkdir -p build_static bin
    cd build_static
    cmake -DCMAKE_BUILD_TYPE=Release -DVERSION="$VERSION" -DSTATIC_LINK=ON ..
    cmake --build . --config Release -j
    cd ..
    cp build_static/pulse_gen bin/pulse_gen_static || true
    
    # Create release package with versioning
    ./package_release.sh
    
    # Update release README with version
    sed -i "s/PULSE VIDEO GENERATOR - Static Release/PULSE VIDEO GENERATOR - Static Release ($VERSION)/" release/README.txt
    sed -i "1i# Version: $VERSION" release/README.txt
    
    # Create zip archive with proper directory structure including platform
    cd release
    RELEASE_DIR_NAME="pulse_gen_${VERSION//v/}_$PLATFORM"
    mkdir -p "$RELEASE_DIR_NAME"
    mv pulse_gen pulse_gen.sh README.txt "$RELEASE_DIR_NAME"/
    zip -r "../pulse_gen-$VERSION-$PLATFORM.zip" "$RELEASE_DIR_NAME"
    cd ..
    
    echo "Release package created: pulse_gen-$VERSION-$PLATFORM.zip"
    echo "Contents include static executable and wrapper script"
else
    echo "Usage:"
    echo "  $0                                     # Build normally"
    echo "  $0 release <version> <platform>        # Create release zip (e.g., $0 release v0.0.1 linux64)"
    echo "  Supported platforms: linux64, linux32, linux-arm64, linux-arm32, win64, win32, win-arm64, user"
fi

