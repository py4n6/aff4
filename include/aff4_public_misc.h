#include <zlib.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/aes.h>
#include <setjmp.h>

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
#include <error.h>
#include <errno.h>
#include <pthread.h>
#include <setjmp.h>
#include <fcntl.h>

#define O_BINARY 0
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif


// Some required definitions from class.h if not already used
#ifndef ___CLASS_H
typedef struct Object_t *Object;

struct Object_t {
  Object __class__;
  Object __super__;
  char *__name__;
  char *__doc__;
  int __size;
  void *extension;
};

extern struct Object_t __Object;

/** Find out if obj is an instance of cls or a derived class.

    Use like this:

    if(issubclass(obj, (Object)&__FileLikeObject)) {
       ...
    };


    You can also do this in a faster way if you already know the class
    hierarchy (but it could break if the hierarchy changes):
    {
     Object cls = ((Object)obj)->__class__;

     if(cls == (Object)&__Image || \
        cls == (Object)&__FileLikeObject || \
        cls == (Object)&__AFFObject || ....) {
     ...
     };
    };
 */
int issubclass(Object obj, Object cls);

extern void unimplemented(Object self);
#endif

// Some required definitions from raptor if not already defined
#ifndef RAPTOR_H
typedef enum {
  RAPTOR_IDENTIFIER_TYPE_UNKNOWN,
  RAPTOR_IDENTIFIER_TYPE_RESOURCE,
  RAPTOR_IDENTIFIER_TYPE_ANONYMOUS,
  RAPTOR_IDENTIFIER_TYPE_PREDICATE,
  RAPTOR_IDENTIFIER_TYPE_ORDINAL,
  RAPTOR_IDENTIFIER_TYPE_LITERAL,
  RAPTOR_IDENTIFIER_TYPE_XML_LITERAL
} raptor_identifier_type;

typedef void* raptor_uri;
typedef void* raptor_serializer;
typedef void* raptor_iostream;
typedef void* raptor_statement;
typedef void* raptor_locator;
#endif

#ifndef __TDB_H__
typedef struct TDB_DATA {
	unsigned char *dptr;
	size_t dsize;
} TDB_DATA;
#endif

typedef void* Queue;
typedef void* StringIO;

// A helper to access the URN of an object.
#define URNOF(x)  ((AFFObject)x)->urn
#define STRING_URNOF(x) ((char *)URNOF(x)->value)
