/*
 * Debugging aid for the Vita port: every C++ exception thrown is reported on stderr with its type
 * and a backtrace, so an "uncaught exception" crash in Vita3K can be traced to the line that threw:
 *
 *   arm-vita-eabi-addr2line -f -C -e vita/build/src/wesnoth <addresses>
 *
 * Hooked in with -Wl,--wrap=__cxa_throw (see vita/wesnoth-vita.cmake).
 */
#include <cstdint>
#include <cstdio>
#include <typeinfo>

extern "C" {

/*
 * The ARM EHABI unwinder, from libgcc. VitaSDK's GCC does not ship unwind.h, so the few parts used
 * here are declared by hand.
 */
struct _Unwind_Context;
enum { URC_NO_REASON = 0, URC_END_OF_STACK = 5 };
enum { UVRSC_CORE = 0, UVRSD_UINT32 = 0, ARM_PC = 15 };
int _Unwind_Backtrace(int (*trace)(_Unwind_Context *, void *), void *arg);
int _Unwind_VRS_Get(_Unwind_Context *context, int reg_class, std::uint32_t reg, int representation, void *value);

[[noreturn]] void __real___cxa_throw(void *object, std::type_info *type, void (*destructor)(void *));

static int print_frame(_Unwind_Context *context, void *depth)
{
	int &n = *static_cast<int *>(depth);
	if (n++ >= 24) {
		return URC_END_OF_STACK;
	}
	std::uint32_t pc = 0;
	_Unwind_VRS_Get(context, UVRSC_CORE, ARM_PC, UVRSD_UINT32, &pc);
	// The low bit only marks Thumb code.
	std::fprintf(stderr, " %08x", static_cast<unsigned>(pc & ~1u));
	return URC_NO_REASON;
}

[[noreturn]] void __wrap___cxa_throw(void *object, std::type_info *type, void (*destructor)(void *))
{
	std::fprintf(stderr, "[vita] throw %s at", type ? type->name() : "?");
	int depth = 0;
	_Unwind_Backtrace(print_frame, &depth);
	std::fprintf(stderr, "\n");
	__real___cxa_throw(object, type, destructor);
}


#ifdef VITA_DEBUG_INPUT
// Who starts text input - which on the Vita means the system keyboard.
void __real_SDL_StartTextInput(void);
void __wrap_SDL_StartTextInput(void)
{
	std::fprintf(stderr, "[vita] SDL_StartTextInput from");
	int depth = 0;
	_Unwind_Backtrace(print_frame, &depth);
	std::fprintf(stderr, "\n");
	__real_SDL_StartTextInput();
}
#endif

}

/** Prints "[vita] <what> at <backtrace>" (for finding who calls something). */
extern "C" void vita_print_backtrace(const char* what)
{
	std::fprintf(stderr, "[vita] %s at", what);
	int depth = 0;
	_Unwind_Backtrace(print_frame, &depth);
	std::fprintf(stderr, "\n");
}
