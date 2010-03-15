#include "encode.h"
#include <string.h>

#define LUT_SIZE 256
#define STUFF_CHAR 'a'

static unsigned char *encode_lut;
static unsigned char *decode_lut;

static unsigned char *encode64_lut;
static unsigned char *decode64_lut;

static unsigned char *urlsafe_encode64_lut;
static unsigned char *urlsafe_decode64_lut;

/* Build the luts */
void encode_init() {
  unsigned char i;
  int k,j=0;

  //Prepare the LUT buffers and clear them:
  encode_lut = (unsigned char *)talloc_size(NULL, LUT_SIZE);
  decode_lut = (unsigned char *)talloc_size(NULL, LUT_SIZE);

  encode64_lut = (unsigned char *)talloc_size(NULL, LUT_SIZE);
  decode64_lut = (unsigned char *)talloc_size(NULL, LUT_SIZE);

  urlsafe_encode64_lut = (unsigned char *)talloc_size(NULL, LUT_SIZE);
  urlsafe_decode64_lut = (unsigned char *)talloc_size(NULL, LUT_SIZE);

  memset(decode_lut,' ',LUT_SIZE);
  memset(encode_lut,' ',LUT_SIZE);
  memset(decode64_lut,' ',LUT_SIZE);
  memset(encode64_lut,' ',LUT_SIZE);
  memset(urlsafe_decode64_lut,' ',LUT_SIZE);
  memset(urlsafe_encode64_lut,' ',LUT_SIZE);

  for(i='z';i>='b';i--,j++) encode_lut[j]=i;
  for(i='9';i>='0';i--,j++) encode_lut[j]=i;
  for(i='A';i<='Z';i++,j++) encode_lut[j]=i;

  j=0;
  for(i='A';i<='Z';i++,j++) encode64_lut[j]=i;
  for(i='a';i<='z';i++,j++) encode64_lut[j]=i;
  for(i='0';i<='9';i++,j++) encode64_lut[j]=i;
  j--;

  j=0;
  for(i='A';i<='Z';i++,j++) urlsafe_encode64_lut[j]=i;
  for(i='a';i<='z';i++,j++) urlsafe_encode64_lut[j]=i;
  for(i='0';i<='9';i++,j++) urlsafe_encode64_lut[j]=i;
  j--;

  encode64_lut[j+1] = '+'; urlsafe_encode64_lut[j+1] = '-'; encode_lut[j++]='/';
  encode64_lut[j+1] = '/'; urlsafe_encode64_lut[j+1] = '_'; encode_lut[j++]='-';
  encode_lut[j++]='+';

  //Now make the reverse map:
  for(k=0;k<j;k++) {
    decode_lut[encode_lut[k]]=k;
    decode64_lut[encode64_lut[k]]=k;
    urlsafe_decode64_lut[urlsafe_encode64_lut[k]]=k;
  }
}

void encodeblock( unsigned char *lut, unsigned char in[3], unsigned char out[4] ) {
  out[0] = lut[ in[0] >> 2 ];
  out[1] = lut[ ((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4) ];
  out[2] = lut[ ((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6) ];
  out[3] = lut[ in[2] & 0x3f ];
}

void decodeblock( unsigned char *lut, unsigned char in[4], unsigned char out[3], int left) {
  switch(left) {
  default:
    out[2] = ((lut[in[2]] << 6) & 0xc0) | lut[in[3]];
  case 2:
    out[1] = lut[in[1]] << 4 | lut[in[2]] >> 2;
  case 1:
    out[0] = lut[in[0]] << 2 | lut[in[1]] >> 4;
  case 0:
    break;
  };
}

int encode64(unsigned char *inbuf,int len, char *outbuf, int outlen) {
  //i is index into inbuf, o index into outbuf
  int i=0,o=0;
  unsigned char *inptr, *outptr;

  memset(outbuf, 0, len * 2);

  inptr = inbuf;
  outptr = (unsigned char *)outbuf;
  while(i<len && o<outlen) {
    encodeblock(encode64_lut, inptr, outptr);
    inptr += 3;
    outptr += 4;
    i+=3;
    o+=4;
  };

  //Encode the remainder
  switch(i-len) {
  case 2:
    outbuf[o-2]='=';
  case 1:
    outbuf[o-1]='=';
  };

  // Null terminate the string
  outbuf[o] = 0;
  return o;
};

/* Decodes inbuf into outbuf (which must be as big) 
   return the length of outbuf.
*/
int decode64(char *inbuf,int len, unsigned char *outbuf, int outlen) {
  int i=0,o=0;
  unsigned char *inptr, *outptr;
  
  inptr = (unsigned char *)inbuf;
  outptr = outbuf;
  while(i<len && o<outlen) {
    decodeblock(decode64_lut, inptr, outptr, outlen-o);
    inptr += 4;
    outptr += 3;
    i+=4;
    o+=3;
  };
  
  if(inbuf[i-1]=='=') o--;
  if(inbuf[i-2]=='=') o--;
  
  return o;
};


/** These are not needed at the moment */
#if 0
/* This encodes inbuf into outbuf. Note we assume that outbuf is 33%
   bigger than inbuf. FIXME: This should probably be realloced here
   just to make sure. len is the size of inbuf.
*/
int encode64(unsigned char *inbuf,int len, unsigned char *outbuf) {
  //i is index into inbuf, o index into outbuf
  int i=0,o=0;
  unsigned int temp;
  
  while(i<len) {
    temp = (unsigned int)inbuf[i] | 
      (unsigned int)inbuf[i+1] << 8 |
      (unsigned int)inbuf[i+2] << 16;

    outbuf[o+0]=encode_lut[temp & 0x3f ];
    outbuf[o+1]=encode_lut[(temp >> 6) & 0x3f ];
    outbuf[o+2]=encode_lut[(temp >> 12) & 0x3f ];
    outbuf[o+3]=encode_lut[(temp >> 18) & 0x3f ];

    i+=3;
    o+=4;
  };
  
  //Encode the remainder
  switch(i-len) {
  case 2:
    outbuf[o-2]=STUFF_CHAR;
    
  case 1:
    outbuf[o-1]=STUFF_CHAR;
  };
  return o;
};

/* Decodes inbuf into outbuf (which must be as big) 
   return the length of outbuf.
*/
int decode64(unsigned char *inbuf,int len, unsigned char *outbuf) {
  int i=0,o=0;
  unsigned int temp;

  while(i<len) {
    temp = 0;

    temp |= decode_lut[(int)inbuf[i+0]];
    temp |= decode_lut[(int)inbuf[i+1]] << 6 ;
    temp |= decode_lut[(int)inbuf[i+2]] << 12 ;
    temp |= decode_lut[(int)inbuf[i+3]] << 18 ;

    outbuf[o+0] = temp & 0xff;
    outbuf[o+1] = (temp & 0xffff) >> 8;
    outbuf[o+2] = (temp & 0xffffff) >> 16;
    outbuf[o+3] = temp >> 24;

    i+=4;
    o+=3;
  };

  if(inbuf[i-1]==STUFF_CHAR) o--;
  if(inbuf[i-2]==STUFF_CHAR) o--;

  return o;
};
#endif

/* This encodes using base32. This has a 60% overhead, but only uses
   32 output chars.

   Note that we use the same luts as base64, which is why they are not
   standard. for base 32 we assume that the data is case insensitive
   and so we use a-z and 0-9.

   base32 takes 5 bytes into 8
 */
int encode32(unsigned char *inbuf, int len, unsigned char *outbuf, int outlen) {
  int i=0,o=0;
  uint64_t temp;
  unsigned char workspace[5];
  int j=0;
  
  while(i<len) {
    temp=0;
    memset(workspace,0,sizeof(workspace));
    memcpy(workspace, inbuf+i, min(len-i,sizeof(workspace)));

    for(j=0;j<5;j++)
      temp |= ((uint64_t)workspace[j]) << (j*8);

    /*
    temp |= ((uint64_t)inbuf[i+0]) << 0;
    temp |= ((uint64_t)inbuf[i+1]) << 8;
    temp |= ((uint64_t)inbuf[i+2]) << 16;
    temp |= ((uint64_t)inbuf[i+3]) << 24;
    temp |= ((uint64_t)inbuf[i+4]) << 32;
    */

    for(j=0; j<8; j++) {
      int offset = ((int)(temp >> (j*5)))  & 0x1f;
      outbuf[o+j]=encode_lut[offset];
    };

    /*
    outbuf[o+0]=encode_lut[(temp >> 0)  & 0x1f];
    outbuf[o+1]=encode_lut[(temp >> 5)  & 0x1f];
    outbuf[o+2]=encode_lut[(temp >> 10) & 0x1f];
    outbuf[o+3]=encode_lut[(temp >> 15) & 0x1f];
    outbuf[o+4]=encode_lut[(temp >> 20) & 0x1f];
    outbuf[o+5]=encode_lut[(temp >> 25) & 0x1f];
    outbuf[o+6]=encode_lut[(temp >> 30) & 0x1f];
    outbuf[o+7]=encode_lut[(temp >> 35) & 0x1f];
    */

    i+=5;
    o+=8;
  };

  //Encode the remainder
  switch(i-len) {
  case 4:
    outbuf[o-4]=STUFF_CHAR;

  case 3:
    outbuf[o-3]=STUFF_CHAR;

  case 2:
    outbuf[o-2]=STUFF_CHAR;

  case 1:
    outbuf[o-1]=STUFF_CHAR;
  };

  outbuf[o]=0;
  return o;
};

int decode32(unsigned char *inbuf, int len, unsigned char *outbuf, int outlen) {
  int i=0,o=0;
  uint64_t temp;
  int j;

  while(i<len) {
    temp = 0;

    for(j=0;j<8;j++)
      temp |= ((uint64_t )decode_lut[(int)inbuf[i+j]]) << (j*5);

    /*
    temp |= ((uint64_t )decode_lut[(int)inbuf[i+0]]) << 0  ;
    temp |= ((uint64_t )decode_lut[(int)inbuf[i+1]]) << 5  ;
    temp |= ((uint64_t )decode_lut[(int)inbuf[i+2]]) << 10 ;
    temp |= ((uint64_t )decode_lut[(int)inbuf[i+3]]) << 15 ;
    temp |= ((uint64_t )decode_lut[(int)inbuf[i+4]]) << 20 ;
    temp |= ((uint64_t )decode_lut[(int)inbuf[i+5]]) << 25 ;
    temp |= ((uint64_t )decode_lut[(int)inbuf[i+6]]) << 30 ;
    temp |= ((uint64_t )decode_lut[(int)inbuf[i+7]]) << 35 ;
    */

    for(j=0;j<5;j++)
      outbuf[o+j] = (temp >> (j*8))  & 0xff;

    /*
    outbuf[o+0] = (temp >> 0)  & 0xff;
    outbuf[o+1] = (temp >> 8)  & 0xff;
    outbuf[o+2] = (temp >> 16) & 0xff;
    outbuf[o+3] = (temp >> 24) & 0xff;
    outbuf[o+4] = (temp >> 32) & 0xff;
    */

    i+=8;
    o+=5;
  };

  for(j=1;j<5;j++)
    if(inbuf[i-j]==STUFF_CHAR) o--;

  /*
  if(inbuf[i-1]==STUFF_CHAR) o--;
  if(inbuf[i-2]==STUFF_CHAR) o--;
  if(inbuf[i-3]==STUFF_CHAR) o--;
  if(inbuf[i-4]==STUFF_CHAR) o--;
  */

  return o;
};

int encodehex(unsigned char *inbuf, int len, unsigned char *outbuf) {
  int i;

  for(i=0;i<len;i++) {
    sprintf((char *)outbuf + i*2, "%02x", inbuf[i]);
  };

  return i*2;
};
