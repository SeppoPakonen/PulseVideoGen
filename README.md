# pulse_perlin_overlay_v2

Self-contained C++17 generator that renders a *white, BPM-synced* 3D Perlin noise pulse video.
No input video. No audio. Frames are piped to **ffmpeg** (CLI) and encoded with libx264.

## Dependencies
- A working `ffmpeg` binary in PATH (with libx264)
- A C++17 toolchain + CMake

## Build

```
./build.sh
```
This creates `build/` and `bin/`, and copies `bin/pulse_gen`.

## Usage

```
bin/pulse_gen --bpm <float> --out output.mp4
[--duration 180] [--fps 30] [--width 1080] [--height 1920]
[--threads auto] [--noise-scale 1.0] [--octaves 6]
[--persistence 0.5] [--lacunarity 2.0] [--seed 1462735277]
[--strength 1.0]
```

- **--bpm** *(float, required)*: musical tempo.
- **--duration** seconds *(default 180)*.
- **--fps** *(default 30)*.
- **--width/--height** output resolution *(default 1080x1920)*.
- **--threads** number of worker threads. Default = `max(1, CPU_COUNT-1)`.
- **--noise-scale** spatial scale factor for Perlin.
- **--octaves/persistence/lacunarity** fractal noise controls.
- **--seed** 32-bit seed (default `0x578437ad`).
- **--strength** global multiplier 0..1 on the envelope*noise.

## What it does
For each frame at time `t = frame_index / fps`:
- envelope: `E(t) = 1 - |sin(2π * (bpm/60) * t)|`
- noise: fractal 3D Perlin `N(x,y,t*0.25)` normalized to `[0,1]`
- pixel intensity (grayscale white): `I = 255 * E(t) * N * strength`

The program spawns `ffmpeg` and streams raw RGB24 frames via stdin:

```
ffmpeg -y -f rawvideo -pix_fmt rgb24 -s WxH -r FPS -i -
-c:v libx264 -pix_fmt yuv420p -preset veryfast -crf 18 -movflags +faststart output.mp4
```

You can tweak `-preset`, `-crf`, etc. via env `FFMPEG_EXTRA_ARGS`.

## Example

```
bin/pulse_gen --bpm 123.5 --duration 90 --out pulse.mp4 --strength 0.7
```


## Notes
- Pipeline C (raw → ffmpeg pipe). No OpenCV. No audio.
- A reference shader is included at `src/shader.glsl` (not used at runtime).
