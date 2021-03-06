#include "aff4_internal.h"

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

  snprintf(buffer, BUFF_SIZE, "0x%02lX", (long unsigned int)arg);
  return buffer;
};

static char *illegal_filename_chars = "|?[]\\+<>:%;&\'\",*#";
static char illegal_filename_lut[128];

void init_luts() {
  char *i;

  memset(illegal_filename_lut, 0,
	 sizeof(illegal_filename_lut));

  for(i=illegal_filename_chars;*i;i++)
    illegal_filename_lut[(int)*i]=1;
};

char *escape_filename(void *ctx, const char *filename, unsigned int length) {
  char *buffer;
  int i,j=0;

  if(length==0) 
    length=strlen(filename);

  buffer = talloc_size(ctx, length*3 + 1);
  for(i=0;i<length;i++) {
    char x=filename[i];
    if(x<32 || illegal_filename_lut[(int)x]) {
      sprintf(buffer+j, "%%%02X", x);
      j+=3;
      if(j>BUFF_SIZE-10) break;
    } else {
      buffer[j]=x;
      j++;
    };
  };

  buffer[j]=0;
  return buffer;
};

char *escape_filename_data(void *ctx, XSDString name) {
  return (char *)escape_filename(ctx, (char *)name->value, name->length-1);
};

XSDString unescape_filename(void *ctx, const char *filename) {
  char buffer[BUFF_SIZE];
  XSDString result = new_XSDString(ctx);

  int i,j=0;
  int length = strlen(filename);

  for(i=0;i<min(length, BUFF_SIZE-10);i++) {
    if(filename[i]=='%') {
      char tmp[10];
      memcpy(tmp+1,filename+i,3);
      tmp[0]='0';
      tmp[1]='x';
      tmp[4]=0;
      buffer[j]=parse_int(tmp);
      i+=2;
      j++;
    } else {
      buffer[j]=filename[i];
      j++;
    };
  };

  CALL(result, set, buffer, j);
  return result;
};

int _mkdir(const char *path)
{
  char opath[256];
  char *p;
  size_t len;

  strncpy(opath, path, sizeof(opath));
  len = strlen(opath);
  if(opath[len - 1] == '/')
    opath[len - 1] = '\0';
  for(p = opath; *p; p++)
    if(*p == '/') {
      *p = '\0';
      if(access(opath, F_OK))
	mkdir(opath, S_IRWXU);
      *p = '/';
    }
  if(access(opath, F_OK))         /* if path is not terminated with /
				   */
    mkdir(opath, S_IRWXU);
  else
    return 0;

  return 1;
}

int startswith(const char *haystack, char *needle) {
  if(strlen(haystack)<strlen(needle)) return 0;
  return !memcmp(haystack, ZSTRING_NO_NULL(needle));
};

int endswith(const char *haystack, char *needle) {
  int haylen = strlen(haystack);
  int needlelen = strlen(needle);
  if(haylen<needlelen) return 0;
  return !memcmp(haystack + haylen - needlelen, needle, needlelen);
};

// Turns url into a fully qualified name 
char *normalise_url(char *url) {
  static char normal_url[BUFF_SIZE];

  if(strstr(url, "://")) {
    return url;
  } else {
    snprintf(normal_url, BUFF_SIZE, "file://%s", url);
    return normal_url;
  };

};

TDB_DATA tdb_data_from_string(char *string) {
  TDB_DATA result;

  result.dptr = (unsigned char *)string;
  result.dsize = string ? strlen(string)+1 : 0;

  return result;
};

void raise_python_exception() {
  RaiseError(ERuntimeError, "Debug check");
};

int AFF4_DEBUG_LEVEL = 0;

#ifndef HAVE_HTONLL
// http://www.rkeene.org/viewer/devel/backuppcd-200601171056/htonll.c.htm
// Copyright (C) 2005  Roy Keene backuppcd-bugs@psislidell.com
uint64_t htonll(uint64_t n) {
  uint64_t retval;

#if __BYTE_ORDER == __BIG_ENDIAN
  retval = n;
#else
  retval = ((uint64_t) htonl(n & 0xFFFFFFFFLLU)) << 32;
  retval |= htonl((n & 0xFFFFFFFF00000000LLU) >> 32);
#endif
  return(retval);
}

AFF4_MODULE_INIT(A000_misc) {
  init_luts();
};

#endif  //HAVE_HTONLL
