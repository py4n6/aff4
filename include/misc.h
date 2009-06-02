#ifndef MISC_H
#define MISC_H
#include "config.h"

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

char *normalise_url(char *url);

#if 0
#define DEBUG(x, ...) printf("%s:%d %d: " x, \
			     __FUNCTION__, __LINE__, pthread_self(), ## __VA_ARGS__);
#else
#define DEBUG(x, ...)
#endif

#endif
