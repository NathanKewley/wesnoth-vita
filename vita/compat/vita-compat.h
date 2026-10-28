/*
 * Force-included into every Wesnoth source file built for the Vita (see vita/configure.sh).
 */
#pragma once

/*
 * VitaSDK's GCC limits.h defines LLONG_MAX and friends only for C99-or-later C, not for C++, and
 * <bits/posix_opt.h> pulls it in before anything else can. Lua then decides there is no long long.
 * GCC's limits.h #undefs these before defining them, so a later include does not clash.
 */
#include <limits.h>
#ifndef LLONG_MAX
#define LLONG_MAX __LONG_LONG_MAX__
#define LLONG_MIN (-LLONG_MAX - 1LL)
#define ULLONG_MAX (LLONG_MAX * 2ULL + 1ULL)
#endif

/*
 * The Vita's OpenSSL is 1.0.2, from before EVP_MD_CTX_new/free were added; these are the 1.0 names
 * for the same calls.
 */
#define EVP_MD_CTX_new EVP_MD_CTX_create
#define EVP_MD_CTX_free EVP_MD_CTX_destroy
