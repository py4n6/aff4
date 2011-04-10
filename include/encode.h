#ifndef _ENCODE_H
#define _ENCODE_H
#include <aff4_internal.h>

/* binary encoding functions.
      
   We assume output buffer to be large enough to accept the encoding.
*/
int encode64(unsigned char *inbuf, int len, char *outbuf, int outlen);
int decode64(char *inbuf, int len, unsigned char *outbuf, int outlen);
int encode32(unsigned char *inbuf, int len, unsigned char *outbuf, int outlen);
int decode32(unsigned char *inbuf, int len, unsigned char *outbuf, int outlen);
int encodehex(unsigned char *inbuf, int len, unsigned char *outbuf);
void encode_init();

#endif
