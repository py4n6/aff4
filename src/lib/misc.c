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


static char *illegal_filename_chars = "|?[]\\=+<>:;\'\",*";
static char illegal_filename_lut[128];
void init_luts() {
  char *i;
  
  memset(illegal_filename_lut, 0, 
	 sizeof(illegal_filename_lut));

  for(i=illegal_filename_chars;*i;i++) 
    illegal_filename_lut[*i]=1;
};

char *escape_filename(char *filename) {
  static char buffer[BUFF_SIZE];

  int i,j=0;
  int length = strlen(filename)+1;
  
  for(i=0;i<length;i++) {
    char x=filename[i];
    if(illegal_filename_lut[x] || x>128) {
      sprintf(buffer+j, "%%%02X", x);
      j+=3;
      if(j>BUFF_SIZE-10) break;
    } else {
      buffer[j]=x;
      j++;
    };
  };

  return buffer;
};

char *unescape_filename(char *filename) {
  static char buffer[BUFF_SIZE];

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
  return buffer;
};
