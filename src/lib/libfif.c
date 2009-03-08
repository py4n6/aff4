#include "fif.h"
#include "zip.h"
#include "time.h"
#include <uuid/uuid.h>
#include <libgen.h>

uint64_t parse_int(char *string) {
  char *endptr;
  uint64_t result;

  if(!string) return 0;

  result = strtoll(string, &endptr, 0);
  switch(*endptr) {
  case 's':
    result *= 512;
    break;
  case 'K':
  case 'k':
    result *= 1024;
    break;
  case 'm':
  case 'M':
    result *= 1024*1024;
    break;
  case 'g':
  case 'G':
    result *= 1024*1024*1024;
    break;
  default:
    break;
  };

  return result;
};

char *from_int(uint64_t arg) {
  static char buffer[BUFF_SIZE];

  snprintf(buffer, BUFF_SIZE, "0x%02llX", arg);
  return buffer;
};
