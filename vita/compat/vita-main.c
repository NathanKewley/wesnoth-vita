/*
 * Process-wide settings for the Vita build, read by the Vita's C runtime before main() runs.
 */

/*
 * The newlib heap - everything malloc and new hand out. Fixed at start-up on the Vita, so it is
 * sized for Wesnoth's game config and image caches, leaving the rest of the app's memory budget to
 * vitaGL, which allocates its own blocks for textures and GPU buffers.
 */
int _newlib_heap_size_user = 256 * 1024 * 1024;

/*
 * Wesnoth's WML preprocessor and parser recurse deeply, and the default main thread stack on the
 * Vita is a few hundred KB.
 */
unsigned int sceUserMainThreadStackSize = 8 * 1024 * 1024;
