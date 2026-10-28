#!/usr/bin/env bash
# Configures the Vita build of Wesnoth in vita/build. Run vita/deps/build-deps.sh first.
#
#   vita/configure.sh [extra cmake args...]
#   cmake --build vita/build -j$(nproc)
set -euo pipefail

source "$(cd "$(dirname "$0")" && pwd)/env.sh"

mkdir -p "$VITA_PORT/build"
cd "$VITA_PORT/build"

# vita-compat.h: fixes for the Vita's C library, force-included everywhere.
# _GNU_SOURCE: in strict C++17 mode newlib hides its POSIX declarations (popen, fseeko, mkstemp...),
# which Lua's io and os libraries use.
# Sections + --gc-sections drop unused code, which on a 512 MB machine is memory back.
VITA_FLAGS="-include $VITA_PORT/compat/vita-compat.h -D_GNU_SOURCE -ffunction-sections -fdata-sections ${VITA_EXTRA_FLAGS:-}"

# HARDEN off: it builds position-independent executables, which the Vita's loader cannot relocate.
# -Wl,-q keeps the relocations in the ELF, which vita-elf-create turns into the ones the Vita's loader
# applies: the game is not always loaded at its link address (Vita3K does, real Vitas did not), and
# without them it crashes before main(). VitaSDK's toolchain adds it through CMAKE_CXX_FLAGS, which
# Wesnoth's CMakeLists replaces, so it is given here with the toolchain's other linker flag.
cmake "$VITA_PORT/.." \
	-DOPT=-O2 \
	-DHARDEN=OFF \
	-DCXX_FLAGS_USER="$VITA_FLAGS" \
	-DCMAKE_EXE_LINKER_FLAGS="-Wl,-q -Wl,-z,nocopyreloc -Wl,--gc-sections" \
	-DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
	-DPKG_CONFIG_EXECUTABLE="$PKG_CONFIG" \
	-DCMAKE_BUILD_TYPE=Release \
	-DBoost_USE_STATIC_LIBS=ON \
	-DBoost_USE_STATIC_RUNTIME=ON \
	-DENABLE_SERVER=OFF \
	-DENABLE_CAMPAIGN_SERVER=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_NLS=OFF \
	-DENABLE_NOTIFICATIONS=OFF \
	-DENABLE_DESKTOP_ENTRY=OFF \
	-DENABLE_APPDATA_FILE=OFF \
	"$@"
