#include "vita-profile.hpp"

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include <cstdint>
#include <cstdio>
#include <malloc.h>
#include <sys/stat.h>

namespace vita_profile
{
namespace
{
const char* const names[STAGE_COUNT] = {"update", "layout", "render", "expose", "present", "idle", "between"};

int state = -1; // unknown, then 0 or 1
std::uint64_t mark = 0;
std::uint64_t window_start = 0;
std::uint64_t spent[STAGE_COUNT] = {};
int frames = 0;
int presented = 0;
// Presented frames that took longer than a 60 Hz refresh allows, and longer than two.
std::uint64_t last_present = 0;
int slow_frames = 0;
int very_slow_frames = 0;

struct named_cost
{
	const char* name;
	int count;
	std::uint64_t us;
	// In the frame being made, and in the slow frames (over 20 ms) of this window.
	std::uint64_t frame_us;
	std::uint64_t slow_us;
};
named_cost costs[128] = {};
std::uint64_t frame_spent[STAGE_COUNT] = {};
std::uint64_t slow_spent[STAGE_COUNT] = {};
std::uint64_t slow_time = 0;

// The scoped_costs open now, innermost last (only on the main thread: drawing happens there).
scoped_cost* open_scopes[32];
int open_count = 0;
// The thread that draws, known from the first frame.
SceUID main_thread = -1;
void add_cost(const char* name, std::uint64_t us, bool count);

std::uint64_t now()
{
	return sceKernelGetProcessTimeWide();
}
} // namespace

bool enabled()
{
	if(state < 0) {
		struct stat st;
		state = stat("/data/wesnoth/vita-profile.txt", &st) == 0 ? 1 : 0;
		window_start = mark = now();
	}
	return state == 1;
}

void frame_begin()
{
	if(!enabled()) {
		return;
	}
	if(main_thread < 0) {
		main_thread = sceKernelGetThreadId();
	}
	lap(OUTSIDE);
}

void lap(stage s)
{
	if(!enabled()) {
		return;
	}
	const std::uint64_t t = now();
	spent[s] += t - mark;
	frame_spent[s] += t - mark;
	mark = t;
}

void frame_end(bool drew)
{
	if(!enabled()) {
		return;
	}
	++frames;
	presented += drew ? 1 : 0;
	const std::uint64_t t = now();
	// Scopes still open have taken this long of this frame; the rest goes to the frames after.
	for(int i = 0; i < open_count; ++i) {
		add_cost(open_scopes[i]->name_, t - open_scopes[i]->start_, false);
		open_scopes[i]->start_ = t;
	}
	// A frame only counts as slow when the one before it was drawn too: after frames with nothing to
	// draw, the time since the last picture is the screen standing still, not a stutter.
	static bool previous_drew = false;
	const bool continuing = previous_drew;
	previous_drew = drew;
	if(drew) {
		const std::uint64_t interval = t - last_present;
		const bool slow = last_present && continuing && interval > 20000;
		slow_frames += slow ? 1 : 0;
		very_slow_frames += slow && interval > 34000 ? 1 : 0;
		last_present = t;
		// What the slow frames spent their time on, against the frame as a whole.
		if(slow) {
			slow_time += interval;
		}
		for(int i = 0; i < STAGE_COUNT; ++i) {
			slow_spent[i] += slow ? frame_spent[i] : 0;
			frame_spent[i] = 0;
		}
		for(named_cost& c : costs) {
			c.slow_us += slow ? c.frame_us : 0;
			c.frame_us = 0;
		}
	}
	if(t - window_start < 2000000) {
		return;
	}
	const double seconds = (t - window_start) / 1e6;
	// Heap in use: the image and text caches never shrink during a scenario.
	const struct mallinfo heap = mallinfo();
	std::fprintf(stderr, "[vita] profile: %.1f fps (%d loops) slow %d/%d heap %d MB |", presented / seconds, frames,
		slow_frames, very_slow_frames, int(heap.uordblks >> 20));
	slow_frames = very_slow_frames = 0;
	for(int i = 0; i < STAGE_COUNT; ++i) {
		std::fprintf(stderr, " %s %.0f%%", names[i], 100.0 * spent[i] / (t - window_start));
		spent[i] = 0;
	}
	for(named_cost& c : costs) {
		if(c.name && (c.count || c.us)) { // (still open: time but no count yet)
			std::fprintf(stderr, " | %s %dx %.0fms", c.name, c.count, c.us / 1000.0);
			c.count = 0;
			c.us = 0;
		}
	}
	std::fprintf(stderr, "\n");
	if(slow_time) {
		std::fprintf(stderr, "[vita] slow frames: %.0f ms |", slow_time / 1000.0);
		for(int i = 0; i < STAGE_COUNT; ++i) {
			std::fprintf(stderr, " %s %.0fms", names[i], slow_spent[i] / 1000.0);
		}
		for(named_cost& c : costs) {
			if(c.name && c.slow_us >= 1000) {
				std::fprintf(stderr, " | %s %.0fms", c.name, c.slow_us / 1000.0);
			}
		}
		std::fprintf(stderr, "\n");
	}
	for(int i = 0; i < STAGE_COUNT; ++i) {
		slow_spent[i] = 0;
	}
	for(named_cost& c : costs) {
		c.slow_us = 0;
	}
	slow_time = 0;
	std::fflush(stderr);
	frames = presented = 0;
	window_start = t;
}
unsigned long long clock_us()
{
	return now();
}

namespace
{
void add_cost(const char* name, std::uint64_t us, bool count)
{
	for(named_cost& c : costs) {
		if(c.name == name || !c.name) {
			c.name = name;
			c.count += count ? 1 : 0;
			c.us += us;
			c.frame_us += us;
			return;
		}
	}
}

bool is_main_thread()
{
	return main_thread >= 0 && sceKernelGetThreadId() == main_thread;
}
} // namespace

void cost(const char* name, unsigned long long us)
{
	if(!enabled()) {
		return;
	}
	add_cost(name, us, true);
}

scoped_log::~scoped_log()
{
	const unsigned long long end = clock_us();
	std::fprintf(stderr, "[vita] %s took %llu ms (done at %llu ms)\n", name_, (end - start_) / 1000, end / 1000);
}

scoped_cost::scoped_cost(const char* name)
	: name_(name)
	, start_(clock_us())
{
	if(enabled() && is_main_thread() && open_count < 32) {
		open_scopes[open_count++] = this;
	}
}

scoped_cost::~scoped_cost()
{
	if(!enabled()) {
		return;
	}
	if(open_count > 0 && open_scopes[open_count - 1] == this) {
		--open_count;
	}
	add_cost(name_, clock_us() - start_, true);
}
} // namespace vita_profile

/*
 * Time spent in SDL's renderer, by call. Hooked in with -Wl,--wrap (see wesnoth-vita.cmake). SDL
 * queues draw commands and runs them on the GPU side when the target changes or the frame is
 * presented, so that cost shows up under SDL_SetRenderTarget, SDL_RenderPresent and the like.
 */
#include <SDL2/SDL.h>

#define VITA_TIMED(ret, fn, params, args) \
	extern "C" ret __real_##fn params; \
	extern "C" ret __wrap_##fn params \
	{ \
		const unsigned long long start = vita_profile::clock_us(); \
		ret result = __real_##fn args; \
		vita_profile::cost(#fn, vita_profile::clock_us() - start); \
		return result; \
	}

VITA_TIMED(int, SDL_SetRenderTarget, (SDL_Renderer* r, SDL_Texture* t), (r, t))
VITA_TIMED(int, SDL_RenderCopy, (SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* s, const SDL_Rect* d), (r, t, s, d))
VITA_TIMED(int, SDL_RenderCopyEx, (SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* s, const SDL_Rect* d, double a, const SDL_Point* c, SDL_RendererFlip f), (r, t, s, d, a, c, f))
VITA_TIMED(int, SDL_RenderFillRect, (SDL_Renderer* r, const SDL_Rect* rect), (r, rect))
VITA_TIMED(int, SDL_RenderDrawLine, (SDL_Renderer* r, int x1, int y1, int x2, int y2), (r, x1, y1, x2, y2))
VITA_TIMED(int, SDL_RenderDrawPoint, (SDL_Renderer* r, int x, int y), (r, x, y))
VITA_TIMED(int, SDL_RenderSetClipRect, (SDL_Renderer* r, const SDL_Rect* rect), (r, rect))
VITA_TIMED(int, SDL_RenderSetViewport, (SDL_Renderer* r, const SDL_Rect* rect), (r, rect))
VITA_TIMED(int, SDL_RenderReadPixels, (SDL_Renderer* r, const SDL_Rect* rect, Uint32 f, void* p, int pitch), (r, rect, f, p, pitch))
VITA_TIMED(int, SDL_UpdateTexture, (SDL_Texture* t, const SDL_Rect* rect, const void* p, int pitch), (t, rect, p, pitch))
VITA_TIMED(SDL_Texture*, SDL_CreateTexture, (SDL_Renderer* r, Uint32 f, int a, int w, int h), (r, f, a, w, h))
VITA_TIMED(SDL_Texture*, SDL_CreateTextureFromSurface, (SDL_Renderer* r, SDL_Surface* s), (r, s))
VITA_TIMED(int, SDL_RenderClear, (SDL_Renderer* r), (r))
VITA_TIMED(int, SDL_SetRenderDrawColor, (SDL_Renderer* r, Uint8 cr, Uint8 g, Uint8 b, Uint8 a), (r, cr, g, b, a))
