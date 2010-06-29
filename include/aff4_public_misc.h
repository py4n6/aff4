#include <zlib.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/aes.h>
#include <setjmp.h>

// Some required definitions from class.h if not already used
#ifndef ___CLASS_H
typedef struct Object_t *Object;

struct Object_t {
  Object __class__;
  Object __super__;
  char *__name__;
  char *__doc__;
  int __size;
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
