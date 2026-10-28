/*
 * PS Vita controls for Wesnoth, which only knows mouse and keyboard.
 *
 * The touchscreen needs nothing here: SDL turns touches into mouse events. The buttons are turned
 * into mouse and keyboard events:
 *
 *   left stick      move a pointer           Cross     left click
 *   D-pad / r.stick arrow keys (scroll)      Circle    right click
 *   L / R           zoom out / in            Square    next unit (n)
 *   Start           Escape (menu, cancel)    Triangle  Enter (confirm)
 *   Select          end turn (Ctrl+Space)
 */
#pragma once

struct SDL_Renderer;

namespace vita_input
{
/** Turns the buttons' state into events. Called at the start of every events::pump(). */
void pump();

/**
 * Draws the stick-driven pointer over the finished frame. The Vita has no mouse cursor of its own,
 * so without this the pointer is invisible. Hidden again once the screen is touched.
 */
void draw_pointer(SDL_Renderer* renderer);
}
