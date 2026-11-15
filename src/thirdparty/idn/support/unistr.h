#ifndef VACUUM_IDN_UNISTR_H
#define VACUUM_IDN_UNISTR_H

#include <stddef.h>
#include <stdint.h>

// Only this gnulib API is needed by libidn's NFKC UTF-8 entry points.
const uint8_t *u8_check(const uint8_t *text, size_t length);

#endif
