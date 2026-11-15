#ifndef VACUUM_IDN_INT_H
#define VACUUM_IDN_INT_H

#include <stdint.h>
#include <stddef.h>

// MSVC supplies fixed-width integers but does not define POSIX ssize_t.
#if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
typedef ptrdiff_t ssize_t;
#define _SSIZE_T_DEFINED
#endif

#endif
