#include "time.h"
#include <uuid/uuid.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include "misc.h"
#include <tdb.h>
#include "aff4.h"

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

static char *illegal_filename_chars = "|?[]\\+<>:;\'\",*";
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

TDB_DATA escape_filename_data(void *ctx, TDB_DATA name) {
  TDB_DATA result;

  result.dptr = escape_filename(ctx, name.dptr, name.dsize);
  result.dsize = strlen(result.dptr);

  return result;
};

TDB_DATA unescape_filename(void *ctx, const char *filename) {
  char buffer[BUFF_SIZE];
  TDB_DATA result;

  int i,j=0;
  int length = strlen(filename)+1;
  
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

  result.dptr = (unsigned char *)talloc_strdup(ctx, buffer);
  result.dsize = j;

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
  result.dsize = strlen(string)+1;

  return result;
};

int AFF4_DEBUG_LEVEL = 0;
