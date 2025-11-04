cmake_minimum_required(VERSION 3.15)
project(pulse_perlin_overlay_v2 CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default version if not provided via command line
if(NOT VERSION)
    set(VERSION "v1.0")
endif()

# Configure version header in build directory
configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)

# Add executable
add_executable(pulse_gen
    src/main.cpp
)

# Add the binary directory to include path so version.h can be found
target_include_directories(pulse_gen PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Check if we're building a static executable
if(STATIC_LINK)
    set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

