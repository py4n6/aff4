#ifndef MISC_H
#define MISC_H
#include "config.h"
#include <assert.h>

#ifdef WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <stdio.h>
#define MSG_NOSIGNAL 0
typedef  unsigned long int in_addr_t;
#else
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>

#define O_BINARY 0
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "encode.h"

#define BUFF_SIZE 40960

int _mkdir(const char *path);
void init_luts();

int startswith(const char *haystack, char *needle);
int endswith(const char *haystack, char *needle);

/** Some helpful macros */
#define READ_INT(fd, x)  CALL(fd, read, (char *)&x, sizeof(x))

#define AS_BUFFER(x) &x, sizeof(x)

char *normalise_url(char *url);
extern int AFF4_DEBUG_LEVEL;

#if 1
#define DEBUG(x, ...) if(AFF4_DEBUG_LEVEL>=1){                              \
    printf("%s:%d %d: " x,                                              \
           __FUNCTION__, __LINE__, (int)pthread_self(), ## __VA_ARGS__); \
  };
#else
#define DEBUG(x, ...)
#endif


#define AFF4_LOG(level, msg, ...)                   \
  if(oracle->logger) {                                     \
  char log_buffer[BUFF_SIZE];                                 \
  snprintf(log_buffer, BUFF_SIZE, msg, ## __VA_ARGS__);   \
  CALL(oracle->logger, message, level, log_buffer);                   \
  }

#endif
