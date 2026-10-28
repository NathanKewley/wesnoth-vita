/*
 * Where the frame time goes, for tuning the PS Vita port. Enabled while
 * ux0:data/wesnoth/vita-profile.txt exists; every 2 seconds it logs frames drawn and the time spent
 * in each stage of draw_manager::sparkle(), and between frames (event handling and game logic).
 */
#pragma once

namespace vita_profile
{
enum stage { UPDATE, LAYOUT, RENDER, EXPOSE, PRESENT, IDLE, OUTSIDE, STAGE_COUNT };

bool enabled();

/** Marks the start of a frame: the time since the previous frame's end counts as OUTSIDE. */
void frame_begin();
/** Adds the time since the last mark to the given stage, and moves the mark. */
void lap(stage s);
/** Marks the end of a frame, logging a summary every 2 seconds. */
void frame_end(bool drew);

/** Microseconds since some fixed point, for timing a piece of work. */
unsigned long long clock_us();
/** Adds a timed piece of work under a name (a string literal), reported with the frame summary. */
void cost(const char* name, unsigned long long us);

/** Logs how long the rest of the enclosing scope took, for one-off steps such as loading. */
struct scoped_log
{
	explicit scoped_log(const char* name) : name_(name), start_(clock_us()) {}
	~scoped_log();
	const char* name_;
	unsigned long long start_;
};

/**
 * Times the rest of the enclosing scope as cost(name). A scope that frames are drawn inside (moving
 * and attacking play their animations from within the click) is split at each frame's end, so each
 * frame is charged only the time the scope took in it.
 */
struct scoped_cost
{
	explicit scoped_cost(const char* name);
	~scoped_cost();
	scoped_cost(const scoped_cost&) = delete;
	scoped_cost& operator=(const scoped_cost&) = delete;
	const char* name_;
	unsigned long long start_;
};
}
