/* Force-included into every dependency built for the Vita: POSIX limits newlib leaves out. */
#ifndef VITA_COMPAT_H
#define VITA_COMPAT_H

#include <limits.h>

#ifndef SSIZE_MAX
#define SSIZE_MAX INT_MAX
#endif
#ifndef _POSIX_HOST_NAME_MAX
#define _POSIX_HOST_NAME_MAX 255
#endif
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

#endif
