/***************************************************************
This is the main internal include file for aff4. This is only included
internally within the library. Users should only ever use the file
aff4.h.
****************************************************************/
#include <assert.h>

#define BUFF_SIZE 40960

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
#include <stdlib.h>
#include <time.h>
#include <uuid/uuid.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <tdb.h>
#include <error.h>
#include <errno.h>
#include <pthread.h>
#include <raptor.h>
#include <setjmp.h>
#include <fcntl.h>

#define O_BINARY 0
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include "encode.h"
#include "class.h"
#include "aff4_utils.h"
#include "aff4_rdf.h"
#include "aff4_io.h"
#include "aff4_resolver.h"
#include "aff4_errors.h"
#include "aff4_constants.h"
#include "aff4_errors.h"

// This file defines the rdf side of the AFF4 specification
#include "aff4_crypto.h"
#include "aff4_objects.h"
#include "ewfvolume.h"
#include "aff4_rdf_serialise.h"

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
    printf("%s:%d 0x%X: " x,						\
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

#define AFF4_LOG(level, service, subject, msg, ...)                     \
  if(AFF4_LOGGER) {                                                           \
    char log_buffer[BUFF_SIZE];                                         \
    snprintf(log_buffer, BUFF_SIZE-1, msg, ## __VA_ARGS__);             \
    CALL(AFF4_LOGGER, message, level, service, subject, log_buffer);         \
  } else {                                                              \
    printf(msg, ## __VA_ARGS__); fflush(stdout);                        \
  };

#define True 1
#define False 0

#ifndef HAVE_HTONLL
uint64_t htonll(uint64_t n);
#define ntohll(x) htonll(x)
#endif

#define AFF4_ABORT(fmt, ...) AFF4_LOG(AFF4_LOG_FATAL_ERROR, AFF4_SERVICE_GENERIC, \
                                      NULL,                             \
                                      "FATAL (%s:%u): " fmt "\n\n",     \
                                      __FUNCTION__, __LINE__, ##__VA_ARGS__); abort();

/** Some helper functions used to serialize int to and from URN
    attributes
*/
uint64_t parse_int(char *string);
char *from_int(uint64_t arg);
char *escape_filename(void *ctx, const char *filename, unsigned int length);
char *escape_filename_data(void *ctx, XSDString name);

/* New reference */
XSDString unescape_filename(void *ctx, const char *filename);

TDB_DATA tdb_data_from_string(char *string);

// A helper to access the URN of an object.
#define URNOF(x)  ((AFFObject)x)->urn
#define STRING_URNOF(x) ((char *)URNOF(x)->value)

// Initialization function for the aff4 library. Autogenned in init.c.
void init_aff4();
