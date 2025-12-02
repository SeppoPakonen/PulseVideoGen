#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <iostream>
#include <unordered_map>
#include <chrono>
#include <mutex>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  ifndef M_PI
#    define M_PI 3.14159265358979323846
#  endif
#  define POPEN _popen
#  define PCLOSE _pclose
#else
#  include <unistd.h>
#  define POPEN popen
#  define PCLOSE pclose
#endif

#include "version.h"

static const char* const VERSION = PULSE_VERSION_STR;

struct Args {
	std::string outPath;
	double bpm = -1.0;           // required
	int duration = 180;          // seconds
	int fps = 30;
	int width = 1080;
	int height = 1920;
	int threads = 0;             // 0 -> auto (CPU-1)
	double noiseScale = 1.0;
	int octaves = 6;
	double persistence = 0.5;
	double lacunarity = 2.0;
	uint32_t seed = 0x578437adU;
	double strength = 1.0;
	int beat_skip = 4;           // beats to skip (default 4)
};

static bool parseArgs(int argc, char** argv, Args& a) {
	std::unordered_map<std::string, std::string> kv;
	for (int i = 1; i < argc; ++i) {
		std::string k = argv[i];
		if (k.rfind("--", 0) == 0) {
			if (i + 1 < argc && std::string(argv[i+1]).rfind("--", 0) != 0) {
				kv[k] = argv[++i];
			} else {
				kv[k] = "1";
			}
		}
	}

	// Define get function separately to avoid lambda parsing issues on Windows
	auto get = [&kv](const char* key) -> const char* {
		auto it = kv.find(key);
		return it == kv.end() ? nullptr : it->second.c_str();
	};

	if (const char* v = get("--out")) a.outPath = v;
	if (const char* v = get("--bpm")) a.bpm = std::stod(v);
	if (const char* v = get("--duration")) a.duration = std::stoi(v);
	if (const char* v = get("--fps")) a.fps = std::stoi(v);
	if (const char* v = get("--width")) a.width = std::stoi(v);
	if (const char* v = get("--height")) a.height = std::stoi(v);
	if (const char* v = get("--threads")) a.threads = std::stoi(v);
	if (const char* v = get("--noise-scale")) a.noiseScale = std::stod(v);
	if (const char* v = get("--octaves")) a.octaves = std::stoi(v);
	if (const char* v = get("--persistence")) a.persistence = std::stod(v);
	if (const char* v = get("--lacunarity")) a.lacunarity = std::stod(v);
	if (const char* v = get("--seed")) a.seed = static_cast<uint32_t>(std::stoul(v));
	if (const char* v = get("--strength")) a.strength = std::stod(v);
	if (const char* v = get("--beat-skip")) a.beat_skip = std::stoi(v);

	if (a.outPath.empty() || a.bpm <= 0.0) {
		std::cerr << "Pulse Video Generator " << VERSION << "\n";
		std::cerr << "Copyright Seppo Pakonen (C) 2025\n";
		std::cerr << "Usage: --bpm <float> --out <output.mp4> "
		"[--duration S] [--fps N] [--width W] [--height H] "
		"[--threads T] [--noise-scale S] [--octaves O] "
		"[--persistence P] [--lacunarity L] [--seed N] [--strength K] [--beat-skip N]\n";
		return false;
	}
	if (a.threads <= 0) {
		unsigned hc = std::thread::hardware_concurrency();
		if (hc == 0) hc = 2;
		a.threads = (int)std::max(1u, hc - 1);
	}
	if (a.strength < 0.0) a.strength = 0.0;
	if (a.strength > 1.0) a.strength = 1.0;
	return true;
}

// ---- Hash & Perlin (CPU port) ----
static inline uint32_t mmhash(uint32_t x, uint32_t seed) {
	const uint32_t m = 0x5bd1e995U;
	uint32_t h = seed;
	uint32_t k = x;
	k *= m; k ^= k >> 24; k *= m;
	h *= m; h ^= k;
	h ^= h >> 13; h *= m; h ^= h >> 15;
	return h;
}

static inline uint32_t mmhash3(uint32_t x, uint32_t y, uint32_t z, uint32_t seed) {
	const uint32_t m = 0x5bd1e995U;
	uint32_t h = seed;
	uint32_t k = x; k *= m; k ^= k >> 24; k *= m; h *= m; h ^= k;
	k = y;   k *= m; k ^= k >> 24; k *= m; h *= m; h ^= k;
	k = z;   k *= m; k ^= k >> 24; k *= m; h *= m; h ^= k;
	h ^= h >> 13; h *= m; h ^= h >> 15;
	return h;
}

static inline void gradientDirection(uint32_t h, double& gx, double& gy, double& gz) {
	switch (int(h & 15U)) {
		case 0:  gx=1; gy=1; gz=0; break;
		case 1:  gx=-1; gy=1; gz=0; break;
		case 2:  gx=1; gy=-1; gz=0; break;
		case 3:  gx=-1; gy=-1; gz=0; break;
		case 4:  gx=1; gy=0; gz=1; break;
		case 5:  gx=-1; gy=0; gz=1; break;
		case 6:  gx=1; gy=0; gz=-1; break;
		case 7:  gx=-1; gy=0; gz=-1; break;
		case 8:  gx=0; gy=1; gz=1; break;
		case 9:  gx=0; gy=-1; gz=1; break;
		case 10: gx=0; gy=1; gz=-1; break;
		case 11: gx=0; gy=-1; gz=-1; break;
		case 12: gx=1; gy=1; gz=0; break;
		case 13: gx=-1; gy=1; gz=0; break;
		case 14: gx=0; gy=-1; gz=1; break;
		case 15: gx=0; gy=-1; gz=-1; break;
	}
}

static inline double fade(double t) {
	return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}
static inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

static double perlinNoise3(double x, double y, double z, uint32_t seed) {
	double fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
	double px = x - fx, py = y - fy, pz = z - fz;

	uint32_t ix = static_cast<uint32_t>(fx);
	uint32_t iy = static_cast<uint32_t>(fy);
	uint32_t iz = static_cast<uint32_t>(fz);

	auto dot_grad = [&](uint32_t cx, uint32_t cy, uint32_t cz, double dx, double dy, double dz) {
		double gx, gy, gz;
		uint32_t h = mmhash3(ix + cx, iy + cy, iz + cz, seed);
		gradientDirection(h, gx, gy, gz);
		return gx*dx + gy*dy + gz*dz;
	};

	double v000 = dot_grad(0,0,0, px,   py,   pz);
	double v100 = dot_grad(1,0,0, px-1, py,   pz);
	double v010 = dot_grad(0,1,0, px,   py-1, pz);
	double v110 = dot_grad(1,1,0, px-1, py-1, pz);
	double v001 = dot_grad(0,0,1, px,   py,   pz-1);
	double v101 = dot_grad(1,0,1, px-1, py,   pz-1);
	double v011 = dot_grad(0,1,1, px,   py-1, pz-1);
	double v111 = dot_grad(1,1,1, px-1, py-1, pz-1);

	double u = fade(px), v = fade(py), w = fade(pz);

	double x00 = lerp(v000, v100, u);
	double x10 = lerp(v010, v110, u);
	double x01 = lerp(v001, v101, u);
	double x11 = lerp(v011, v111, u);

	double y0 = lerp(x00, x10, v);
	double y1 = lerp(x01, x11, v);

	double res = lerp(y0, y1, w);
	return res; // ~[-1,1]
}

static double perlinFractal(double x, double y, double z,
							int baseFreq, int octaves, double persistence, double lacunarity, uint32_t seed) {
	double value = 0.0;
	double amplitude = 1.0;
	double freq = double(baseFreq);
	uint32_t curSeed = seed;
	for (int i = 0; i < octaves; ++i) {
		curSeed = mmhash(curSeed, 0x0U);
		value += perlinNoise3(x * freq, y * freq, z * freq, curSeed) * amplitude;
		amplitude *= persistence;
		freq *= lacunarity;
	}
	return value;
}

// ---- Rendering ----
struct RenderCtx {
	int W, H;
	double aspect;
	double bpm;
	int beat_skip;
	double noiseScale;
	int octaves;
	double persistence;
	double lacunarity;
	uint32_t seed;
	double strength;
	double z_speed;
};

static void render_rows(uint8_t* rgb, int W, int H, int y0, int y1,
						const RenderCtx& ctx, double t) {
	const double effective_bpm = ctx.bpm / static_cast<double>(ctx.beat_skip);
	const double omega = 2.0 * M_PI * (effective_bpm / 60.0);
	const double phase = omega * t;
	const double envelope = 1.0 - std::abs(std::sin(phase));
	const double z = t * ctx.z_speed;

	for (int y = y0; y < y1; ++y) {
		for (int x = 0; x < W; ++x) {
			double u = (double(x) + 0.5) / double(W);
			double v = (double(y) + 0.5) / double(H);
			double px = u * ctx.aspect * ctx.noiseScale;
			double py = v * ctx.noiseScale;

			double n = perlinFractal(px, py, z, 1, ctx.octaves, ctx.persistence, ctx.lacunarity, ctx.seed);
			double n01 = (n + 1.0) * 0.5;
			// Apply envelope modulation to the noise value before clamping
			double a = envelope * n01 * ctx.strength;
			// Ensure value is in [0,1] range
			if (a < 0.0) a = 0.0;
			if (a > 1.0) a = 1.0;
			uint8_t val = (uint8_t)std::lround(a * 255.0);

			size_t idx = (size_t(y) * W + x) * 3;
			rgb[idx+0] = val; // R
			rgb[idx+1] = val; // G
			rgb[idx+2] = val; // B
		}
	}
}

int main(int argc, char** argv) {
	Args args;
	if (!parseArgs(argc, argv, args)) return 1;

	const int W = args.width;
	const int H = args.height;
	const int FPS = args.fps;
	const int frames = args.duration * FPS;

	// Prepare ffmpeg command
	std::string extra = "";
	if (const char* e = std::getenv("FFMPEG_EXTRA_ARGS")) extra = e;

	char cmd[4096];
	std::snprintf(cmd, sizeof(cmd),
					"ffmpeg -y -f rawvideo -pix_fmt rgb24 -s %dx%d -r %d -i - "
					"-c:v libx264 -pix_fmt yuv420p -preset veryfast -crf 18 -movflags +faststart %s \"%s\"",
W, H, FPS, extra.c_str(), args.outPath.c_str());

	std::cerr << "Launching: " << cmd << "\n";
	FILE* ff = POPEN(cmd, "w");
	if (!ff) {
		std::cerr << "Failed to spawn ffmpeg.\n";
		return 1;
	}

	std::vector<uint8_t> frame((size_t)W * H * 3);
	RenderCtx ctx { W, H, double(W)/double(H), args.bpm, args.beat_skip, args.noiseScale,
		args.octaves, args.persistence, args.lacunarity,
		args.seed, args.strength, 0.25 };

	const int T = args.threads;
	std::vector<std::thread> workers;  // Start with empty vector

	auto t0 = std::chrono::high_resolution_clock::now();

	for (int f = 0; f < frames; ++f) {
		double t = double(f) / double(FPS);

		// Ensure proper cleanup: join any existing threads and clear the vector
		for (auto& th : workers) {
			if (th.joinable()) {
				th.join();
			}
		}
		workers.clear();

		// Resize and populate with new threads for this frame
		workers.resize(T);

		// Partition rows - ensure proper synchronization
		int rowsPer = H / T;
		int rem = H % T;
		int y = 0;
		for (int i = 0; i < T; ++i) {
			int take = rowsPer + (i < rem ? 1 : 0);
			int y0 = y;
			int y1 = y + take;
			workers[i] = std::thread(render_rows, frame.data(), W, H, y0, y1, std::ref(ctx), t);
			y = y1;
		}
		for (auto& th : workers) th.join();

		// More comprehensive memory synchronization for Windows
		std::atomic_thread_fence(std::memory_order_seq_cst);

		// Ensure all CPU cores have synchronized their caches
		std::this_thread::sleep_for(std::chrono::microseconds(1));  // Tiny delay to ensure completion

		// Write to ffmpeg stdin with potential buffer flushing
		size_t wrote = fwrite(frame.data(), 1, frame.size(), ff);
		if (wrote != frame.size()) {
			std::cerr << "ffmpeg write failed at frame " << f << "\n";
			break;
		}
		fflush(ff);  // Ensure data is flushed to ffmpeg
	}

	int rc = PCLOSE(ff);
	auto t1 = std::chrono::high_resolution_clock::now();
	double secs = std::chrono::duration<double>(t1 - t0).count();
	std::cerr << "Done. Encoded " << frames << " frames in " << secs << " s.\n";
	return rc;
}
