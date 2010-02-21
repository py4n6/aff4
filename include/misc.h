#ifndef MISC_H
#define MISC_H
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

#define _DEBUG(x, ...) {                                                \
    printf("%s:%d 0x%X: " x,                                              \
           __FUNCTION__, __LINE__, (unsigned int)pthread_self(), ## __VA_ARGS__); \
  };

#ifdef AFF4_DEBUG_LOCKS
#define DEBUG_LOCK(x, ...) _DEBUG(x, ## __VA_ARGS__)
#else
#define DEBUG_LOCK(x, ...)
#endif

#ifdef AFF4_DEBUG_RESOLVER
#define DEBUG_RESOLVER(x, ...) _DEBUG(x, ## __VA_ARGS__)
#else
#define DEBUG_RESOLVER(x, ...)
#endif

#ifdef AFF4_DEBUG_OBJECT
#define DEBUG_OBJECT(x, ...) _DEBUG(x, ## __VA_ARGS__)
#else
#define DEBUG_OBJECT(x, ...)
#endif

#ifdef AFF4_DEBUG_GEN
#define DEBUG_GEN(x, ...) _DEBUG(x, ## __VA_ARGS__)
#else
#define DEBUG_GEN(x, ...)
#endif

#define TDB_DATA_STRING(x)                      \
  (char *)x.dptr, x.dsize

#define AFF4_LOG(level, msg, ...)                   \
  if(oracle && oracle->logger) {                                     \
  char log_buffer[BUFF_SIZE];                                 \
  snprintf(log_buffer, BUFF_SIZE, msg, ## __VA_ARGS__);   \
  CALL(oracle->logger, message, level, log_buffer);                   \
  } else {                                                            \
    printf(msg, ## __VA_ARGS__); fflush(stdout);                      \
  };

#define True 1
#define False 0

#ifndef HAVE_HTONLL
uint64_t htonll(uint64_t n);
#define ntohll(x) htonll(x)
#endif

#define AFF4_ABORT(fmt, ...) AFF4_LOG(10, "FATAL (%s:%u): " fmt "\n\n", \
                                      __FUNCTION__, __LINE__, ##__VA_ARGS__); abort();

#endif
