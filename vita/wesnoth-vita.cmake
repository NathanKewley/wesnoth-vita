# PS Vita: extra libraries for the wesnoth executable, and packaging it as a .vpk.
# Included from src/CMakeLists.txt when building with VitaSDK's toolchain (see vita/configure.sh).

set(VITA_APP_NAME "Wesnoth")
set(VITA_TITLEID "WSNT00001")
# Vita version strings are XX.YY; 1.18 port, revision 0.
set(VITA_VERSION "01.18")
set(VITA_PORT_DIR ${CMAKE_SOURCE_DIR}/vita)

# vita_create_self and vita_create_vpk.
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

target_sources(wesnoth PRIVATE
	${VITA_PORT_DIR}/compat/vita-main.c
	${VITA_PORT_DIR}/compat/icu-stubs.c
	${VITA_PORT_DIR}/compat/vita-debug.cpp
	${VITA_PORT_DIR}/compat/vita-fonts.c
	${VITA_PORT_DIR}/compat/vita-input.cpp
	${VITA_PORT_DIR}/compat/vita-pack.cpp
	${VITA_PORT_DIR}/compat/vita-profile.cpp
)
# Every exception thrown is logged with a backtrace (vita-debug.cpp).
target_link_options(wesnoth PRIVATE -Wl,--wrap=__cxa_throw)
# Fonts are read whole into memory, not a little at a time from the memory card (vita-fonts.c).
target_link_options(wesnoth PRIVATE -Wl,--wrap=FT_New_Face)
target_include_directories(wesnoth PRIVATE $ENV{VITASDK}/arm-vita-eabi/include/freetype2)
# SDL renderer calls are timed for the profiler (vita-profile.cpp).
foreach(fn SDL_SetRenderTarget SDL_RenderCopy SDL_RenderCopyEx SDL_RenderFillRect SDL_RenderDrawLine
		SDL_RenderDrawPoint SDL_RenderSetClipRect SDL_RenderSetViewport SDL_RenderReadPixels SDL_UpdateTexture
		SDL_CreateTexture SDL_CreateTextureFromSurface SDL_RenderClear SDL_SetRenderDrawColor)
	target_link_options(wesnoth PRIVATE -Wl,--wrap=${fn})
endforeach()
if(CMAKE_CXX_FLAGS MATCHES "VITA_DEBUG_INPUT" OR CXX_FLAGS_USER MATCHES "VITA_DEBUG_INPUT")
	target_link_options(wesnoth PRIVATE -Wl,--wrap=SDL_StartTextInput)
endif()

# vita_create_self expects the elf in this directory's build directory; Wesnoth otherwise puts its
# executables in the source tree.
set_target_properties(wesnoth PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# SDL2 for the Vita draws through vitaGL, whose shader compiler needs these after it on the link
# line; SDL2's pkg-config file leaves them out.
#
# Everything is a static library here, where link order matters: ICU's libraries have to come after
# Boost.Locale, which uses them, but Wesnoth lists them first - and CMake drops a library named
# twice, so they go at the very end with these.
set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -licui18n -licuuc -licudata -lSceShaccCgExt -ltaihen_stub")

# libstdc++ decides whether threads exist by checking a weak reference to pthread_cancel, and a weak
# reference does not pull anything out of a static library. Nothing calls pthread_cancel, so without
# this std::call_once (ICU's mutexes use it) fails with "Unknown error" and the game aborts at start.
target_link_options(wesnoth PRIVATE -Wl,--whole-archive -lpthread -Wl,--no-whole-archive)

vita_create_self(wesnoth.self wesnoth UNSAFE)

# ATTRIBUTE2=12: the extended memory budget, which raises the app's share of the Vita's RAM.
set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")

# The game data (~600 MB) is not in the .vpk: it goes to ux0:data/wesnoth/ (see vita/README.md),
# so a rebuilt binary installs in seconds.
vita_create_vpk(wesnoth.vpk ${VITA_TITLEID} wesnoth.self
	VERSION ${VITA_VERSION}
	NAME ${VITA_APP_NAME}
	FILE ${VITA_PORT_DIR}/sce_sys/icon0.png sce_sys/icon0.png
	FILE ${VITA_PORT_DIR}/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
	FILE ${VITA_PORT_DIR}/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
	FILE ${VITA_PORT_DIR}/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
