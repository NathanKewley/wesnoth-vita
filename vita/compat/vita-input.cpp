#include "vita-input.hpp"

#include "display.hpp"
#include "draw_manager.hpp"
#include "gui/core/event/handler.hpp"
#include "picture.hpp"
#include "sdl/rect.hpp"
#include "video.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
// Scripted input (below) is built in always, and on only while its file exists.
#define VITA_SCRIPTED_INPUT
#ifdef VITA_SCRIPTED_INPUT
#include <fstream>
#include <string>
#include <vector>
#endif

namespace vita_input
{
namespace
{
SDL_GameController* pad = nullptr;
bool initialised = false;

bool held[SDL_CONTROLLER_BUTTON_MAX] = {};

/** Right-stick directions currently held down as arrow keys: up, down, left, right. */
bool stick_held[4] = {};

/** The pointer, in window pixels. Fractional so slow stick movement still adds up. */
float pointer_x = 480.0f;
float pointer_y = 272.0f;
bool pointer_shown = false;
Uint32 last_tick = 0;

/**
 * The front touch screen. On the game map a drag pans the map and a tap is a left click; anywhere
 * else (menus, dialogs, the sidebar) the finger is the mouse with its left button held.
 */
struct touch_state
{
	bool down = false;
	/** Panning the map (started on it, no dialog open), rather than acting as the mouse. */
	bool panning = false;
	/** Moved far enough from where it went down to be a drag, not a tap. */
	bool dragged = false;
	float start_x = 0.0f, start_y = 0.0f;
	float last_x = 0.0f, last_y = 0.0f;
} touch;

/** How far, in pixels, a finger may wander and still be a tap. */
constexpr float tap_slop = 12.0f;

/** A modifier set for a key combination; cleared on the next pump, once the key has been handled. */
bool clear_mods_next = false;

/** Stick travel, out of 32767, ignored as noise. */
constexpr int dead_zone = 6000;
/** Pointer speed at full tilt, in pixels per second. */
constexpr float pointer_speed = 700.0f;
constexpr int pointer_size = 32;

Uint32 window_id()
{
	SDL_Window* window = SDL_GetWindowFromID(1);
	return window ? SDL_GetWindowID(window) : 0;
}

void push_key(SDL_Keycode key, bool down)
{
	SDL_Event event{};
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.windowID = window_id();
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.sym = key;
	event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
	event.key.keysym.mod = SDL_GetModState();
	SDL_PushEvent(&event);
}

void tap_key(SDL_Keycode key, SDL_Keymod mods = KMOD_NONE)
{
	if(mods != KMOD_NONE) {
		// Wesnoth reads modifiers from SDL's state, not from the event.
		SDL_SetModState(mods);
		clear_mods_next = true;
	}
	push_key(key, true);
	push_key(key, false);
}

void push_mouse_button(Uint8 button, bool down)
{
	SDL_Event event{};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.windowID = window_id();
	event.button.button = button;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 1;
	event.button.x = static_cast<int>(pointer_x);
	event.button.y = static_cast<int>(pointer_y);
	SDL_PushEvent(&event);
}

rect pointer_rect()
{
	return {static_cast<int>(pointer_x), static_cast<int>(pointer_y), pointer_size, pointer_size};
}

#ifdef VITA_SCRIPTED_INPUT
/*
 * Scripted input, for testing the controls in an emulator nothing can press buttons in. Read from
 * /data/wesnoth/vita-input-test.txt, lines of "<ms since start> <what> ...":
 *
 *   40000 press A            a button tap (A B X Y START BACK LB RB UP DOWN LEFT RIGHT)
 *   41000 stick 1 -0.5 600   the left stick at x,y (-1..1) for 600 ms
 *   42000 key ctrl o         a key press, with ctrl/shift/alt or "-" for none (SDL key names)
 *   43000 touch 0.5 0.5 0.3 0.5 400   a finger on the front screen, moving from x,y to x2,y2 (0..1)
 *                            over 400 ms (the same point twice for a tap)
 */
struct scripted_step
{
	Uint32 at;
	std::string what;
	float x, y;
	float x2 = 0.0f, y2 = 0.0f;
	Uint32 length;
	SDL_Keycode key = SDLK_UNKNOWN;
	Uint16 mods = KMOD_NONE;
	bool done = false;
};
std::vector<scripted_step> script;

void load_script()
{
	std::ifstream in{"/data/wesnoth/vita-input-test.txt"};
	if(!in) {
		return;
	}
	scripted_step step{};
	while(in >> step.at >> step.what) {
		step.x = step.y = 0.0f;
		step.length = 150;
		step.key = SDLK_UNKNOWN;
		step.mods = KMOD_NONE;
		if(step.what == "stick") {
			in >> step.x >> step.y >> step.length;
		} else if(step.what == "touch") {
			in >> step.x >> step.y >> step.x2 >> step.y2 >> step.length;
		} else if(step.what == "press") {
			in >> step.what;
		} else if(step.what == "key") {
			std::string mod, name;
			in >> mod >> name;
			step.mods = mod == "ctrl" ? KMOD_CTRL : mod == "shift" ? KMOD_SHIFT : mod == "alt" ? KMOD_ALT : KMOD_NONE;
			step.key = SDL_GetKeyFromName(name.c_str());
		}
		script.push_back(step);
	}
	std::fprintf(stderr, "[vita] input: %u scripted steps\n", unsigned(script.size()));
}

const scripted_step* scripted(const char* what, Uint32 now)
{
	for(const scripted_step& step : script) {
		if(step.what == what && now >= step.at && now < step.at + step.length) {
			return &step;
		}
	}
	return nullptr;
}

const char* const button_names[SDL_CONTROLLER_BUTTON_MAX] = {
	"A", "B", "X", "Y", "BACK", "GUIDE", "START", "LSTICK", "RSTICK", "LB", "RB", "UP", "DOWN", "LEFT", "RIGHT",
};
#endif

bool button_down(SDL_GameControllerButton button)
{
#ifdef VITA_SCRIPTED_INPUT
	if(!script.empty()) {
		return button < SDL_CONTROLLER_BUTTON_MAX && button_names[button] && scripted(button_names[button], SDL_GetTicks());
	}
#endif
	return SDL_GameControllerGetButton(pad, button) != 0;
}

int raw_axis(SDL_GameControllerAxis which)
{
#ifdef VITA_SCRIPTED_INPUT
	if(!script.empty()) {
		const scripted_step* step = scripted("stick", SDL_GetTicks());
		if(!step) {
			return 0;
		}
		const float v = which == SDL_CONTROLLER_AXIS_LEFTX ? step->x : which == SDL_CONTROLLER_AXIS_LEFTY ? step->y : 0.0f;
		return static_cast<int>(v * 32767);
	}
#endif
	return SDL_GameControllerGetAxis(pad, which);
}

/** Stick deflection past the dead zone, as -1..1. */
float axis(SDL_GameControllerAxis which)
{
	const int value = raw_axis(which);
	if(std::abs(value) < dead_zone) {
		return 0.0f;
	}
	const float sign = value < 0 ? -1.0f : 1.0f;
	return sign * (std::abs(value) - dead_zone) / float(32767 - dead_zone);
}

void move_pointer(float dt)
{
	const float dx = axis(SDL_CONTROLLER_AXIS_LEFTX);
	const float dy = axis(SDL_CONTROLLER_AXIS_LEFTY);
	if(dx == 0.0f && dy == 0.0f) {
		return;
	}

	draw_manager::invalidate_region(pointer_rect());
	// Squared for precision near the centre, still fast at full tilt.
	pointer_x += dx * std::abs(dx) * pointer_speed * dt;
	pointer_y += dy * std::abs(dy) * pointer_speed * dt;
	const point size = video::window_size();
	pointer_x = std::clamp(pointer_x, 0.0f, float(size.x - 1));
	pointer_y = std::clamp(pointer_y, 0.0f, float(size.y - 1));
	pointer_shown = true;
	draw_manager::invalidate_region(pointer_rect());

	if(SDL_Window* window = SDL_GetWindowFromID(window_id())) {
		// Moves SDL's own idea of the mouse too, which is what hover effects and map scrolling read.
		SDL_WarpMouseInWindow(window, int(pointer_x), int(pointer_y));
	}
}

void right_stick_as_arrows()
{
	const float x = axis(SDL_CONTROLLER_AXIS_RIGHTX);
	const float y = axis(SDL_CONTROLLER_AXIS_RIGHTY);
	const bool want[4] = {y < -0.5f, y > 0.5f, x < -0.5f, x > 0.5f};
	static const SDL_Keycode keys[4] = {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
	for(int i = 0; i < 4; ++i) {
		if(want[i] != stick_held[i]) {
			stick_held[i] = want[i];
			push_key(keys[i], want[i]);
		}
	}
}

void move_mouse_to(float x, float y)
{
	pointer_x = x;
	pointer_y = y;
	if(SDL_Window* window = SDL_GetWindowFromID(window_id())) {
		SDL_WarpMouseInWindow(window, int(pointer_x), int(pointer_y));
	}
}

/** Whether a touch at this point is on the game map, with no dialog over it. */
bool on_map(float x, float y)
{
	const display* disp = display::get_singleton();
	return disp && !gui2::is_in_dialog() && disp->map_area().contains(int(x), int(y));
}

void handle_touch()
{
	const SDL_TouchID device = SDL_GetTouchDevice(0);
	const SDL_Finger* finger = device && SDL_GetNumTouchFingers(device) > 0 ? SDL_GetTouchFinger(device, 0) : nullptr;
#ifdef VITA_SCRIPTED_INPUT
	SDL_Finger scripted_finger{};
	if(!script.empty()) {
		const Uint32 now = SDL_GetTicks();
		finger = nullptr;
		if(const scripted_step* step = scripted("touch", now)) {
			const float t = step->length ? float(now - step->at) / step->length : 1.0f;
			scripted_finger.x = step->x + (step->x2 - step->x) * t;
			scripted_finger.y = step->y + (step->y2 - step->y) * t;
			finger = &scripted_finger;
		}
	}
#endif

	if(!finger) {
		if(touch.down) {
			touch.down = false;
			if(!touch.panning) {
				push_mouse_button(SDL_BUTTON_LEFT, false);
			} else if(!touch.dragged) {
				// A tap on the map: a click where it was.
				push_mouse_button(SDL_BUTTON_LEFT, true);
				push_mouse_button(SDL_BUTTON_LEFT, false);
			}
#ifdef VITA_SCRIPTED_INPUT
			if(!script.empty()) {
				std::fprintf(stderr, "[vita] input: touch up at %.0f,%.0f: %s\n", touch.last_x, touch.last_y,
					touch.panning ? (touch.dragged ? "map panned" : "tap on the map, clicked") : "mouse released");
			}
#endif
		}
		return;
	}

	// The panel reports 0..1 across the screen.
	const point size = video::window_size();
	const float x = std::clamp(finger->x * size.x, 0.0f, float(size.x - 1));
	const float y = std::clamp(finger->y * size.y, 0.0f, float(size.y - 1));

	if(!touch.down) {
		// The finger takes over from the stick's pointer, which is hidden until the stick moves again.
		if(pointer_shown) {
			draw_manager::invalidate_region(pointer_rect());
			pointer_shown = false;
		}
		touch = touch_state{true, on_map(x, y), false, x, y, x, y};
#ifdef VITA_SCRIPTED_INPUT
		if(!script.empty()) {
			std::fprintf(stderr, "[vita] input: touch down at %.0f,%.0f, %s\n", x, y, touch.panning ? "on the map" : "as the mouse");
		}
#endif
		move_mouse_to(x, y);
		if(!touch.panning) {
			push_mouse_button(SDL_BUTTON_LEFT, true);
		}
		return;
	}

	if(!touch.dragged && std::hypot(x - touch.start_x, y - touch.start_y) > tap_slop) {
		touch.dragged = true;
	}
	if(touch.panning) {
		if(touch.dragged) {
			if(display* disp = display::get_singleton()) {
				// The map follows the finger.
				const bool scrolled = disp->scroll(int(touch.last_x - x), int(touch.last_y - y));
#ifdef VITA_SCRIPTED_INPUT
				if(!script.empty() && (int(touch.last_x - x) || int(touch.last_y - y))) {
					std::fprintf(stderr, "[vita] input: pan %d,%d %s\n", int(touch.last_x - x), int(touch.last_y - y), scrolled ? "scrolled" : "no scroll");
				}
#endif
			}
			touch.last_x = x;
			touch.last_y = y;
		}
	} else if(x != touch.last_x || y != touch.last_y) {
		move_mouse_to(x, y);
		touch.last_x = x;
		touch.last_y = y;
	}
}

void on_button(SDL_GameControllerButton button, bool down)
{
	switch(button) {
	case SDL_CONTROLLER_BUTTON_A:
		push_mouse_button(SDL_BUTTON_LEFT, down);
		break;
	case SDL_CONTROLLER_BUTTON_B:
		push_mouse_button(SDL_BUTTON_RIGHT, down);
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_UP:
		push_key(SDLK_UP, down);
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
		push_key(SDLK_DOWN, down);
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
		push_key(SDLK_LEFT, down);
		break;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
		push_key(SDLK_RIGHT, down);
		break;
	default:
		// The rest act once, on press.
		if(!down) {
			break;
		}
		switch(button) {
		case SDL_CONTROLLER_BUTTON_X:
			tap_key(SDLK_n);
			break;
		case SDL_CONTROLLER_BUTTON_Y:
			tap_key(SDLK_RETURN);
			break;
		case SDL_CONTROLLER_BUTTON_START:
			tap_key(SDLK_ESCAPE);
			break;
		case SDL_CONTROLLER_BUTTON_BACK:
			tap_key(SDLK_SPACE, KMOD_LCTRL);
			break;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
			tap_key(SDLK_MINUS);
			break;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
			tap_key(SDLK_EQUALS);
			break;
		default:
			break;
		}
	}
}
} // namespace

void pump()
{
	if(!initialised) {
		initialised = true;
		// The front touch screen is handled here (handle_touch), not by SDL's own mouse emulation or
		// Wesnoth's finger handling, which would both act on it too.
		SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
		SDL_EventState(SDL_FINGERDOWN, SDL_DISABLE);
		SDL_EventState(SDL_FINGERUP, SDL_DISABLE);
		SDL_EventState(SDL_FINGERMOTION, SDL_DISABLE);
		SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
		pad = SDL_GameControllerOpen(0);
		last_tick = SDL_GetTicks();
#ifdef VITA_SCRIPTED_INPUT
		load_script();
		if(!script.empty()) {
			std::fprintf(stderr, "[vita] input: %d joysticks, controller %s\n", SDL_NumJoysticks(),
				pad ? SDL_GameControllerName(pad) : SDL_GetError());
		}
#endif
	}
	if(!pad) {
		return;
	}

	if(clear_mods_next) {
		SDL_SetModState(KMOD_NONE);
		clear_mods_next = false;
	}

	SDL_GameControllerUpdate();

	const Uint32 now = SDL_GetTicks();
#ifdef VITA_SCRIPTED_INPUT
	for(scripted_step& step : script) {
		if(step.key != SDLK_UNKNOWN && !step.done && now >= step.at) {
			step.done = true;
			SDL_SetModState(static_cast<SDL_Keymod>(step.mods));
			SDL_Event event{};
			event.type = SDL_KEYDOWN;
			event.key.state = SDL_PRESSED;
			event.key.keysym.sym = step.key;
			event.key.keysym.scancode = SDL_GetScancodeFromKey(step.key);
			event.key.keysym.mod = step.mods;
			SDL_PushEvent(&event);
			// Hotkeys on plain characters ("a" for add-ons) go by the text typed, not the key.
			const char* name = SDL_GetKeyName(step.key);
			if(step.mods == KMOD_NONE && name[0] && !name[1]) {
				SDL_Event text{};
				text.type = SDL_TEXTINPUT;
				text.text.text[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[0])));
				SDL_PushEvent(&text);
			}
			event.type = SDL_KEYUP;
			event.key.state = SDL_RELEASED;
			SDL_PushEvent(&event);
			clear_mods_next = true;
			std::fprintf(stderr, "[vita] input: key %s\n", SDL_GetKeyName(step.key));
		}
	}
#endif
	const float dt = std::min(0.1f, (now - last_tick) / 1000.0f);
	last_tick = now;

	for(int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i) {
		const auto button = static_cast<SDL_GameControllerButton>(i);
		const bool down = button_down(button);
		if(down != held[i]) {
			held[i] = down;
#ifdef VITA_SCRIPTED_INPUT
			if(!script.empty()) {
				std::fprintf(stderr, "[vita] input: button %d %s\n", i, down ? "down" : "up");
			}
#endif
			on_button(button, down);
		}
	}

	move_pointer(dt);
	right_stick_as_arrows();
	handle_touch();
}

void draw_pointer(SDL_Renderer* renderer)
{
	if(!pointer_shown) {
		return;
	}
	const texture& arrow = image::get_texture(image::locator{"cursors/normal.png"});
	if(!arrow) {
		return;
	}
	const SDL_Rect dst = pointer_rect();
	SDL_RenderCopy(renderer, arrow.get(), nullptr, &dst);
}
} // namespace vita_input
