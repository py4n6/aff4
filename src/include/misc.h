#ifndef MISC_H
#define MISC_H
#include "config.h"

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define BUFF_SIZE 4096 * 100

#endif
