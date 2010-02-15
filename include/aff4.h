/** This is the public include file for using libaff4.

This defines the API to the AFF4 reference implementation. The API is
divided into two parts - a low level API and a high level API.

The low level API is found in zip.h which describes all the classes
and their interfaces.

This file describes the high level interface.
*/
#ifndef __AFF4_H
#define __AFF4_H

#include <stdint.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_TYPES_H
#include <sys/types.h>
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#endif

#include "aff4_constants.h"

// This file defines the rdf side of the AFF4 specification
#include "aff4_rdf.h"
#include "aff4_utils.h"
#include "aff4_resolver.h"
#include "aff4_crypto.h"
#include "aff4_objects.h"
#include "ewfvolume.h"

/** 
    Opens the URIs specified in images and uses them to populate the
    resolver. We then try to open the stream.

    images may be omitted in which case we will use exiting
    information in the universal resolver.

    If stream is not specified (NULL), we attempt to open the stream
    "default" in each image.

    We return an opaque handle to the stream or NULL if failed.
*/
typedef void * AFF4_HANDLE;

AFF4_HANDLE aff4_open(char **images);

// Same as aff4_open but it does not return an actual stream - this is
// useful to populate the resolver (and then do queries on
// it). returns 1 if any image was loaded.
char **aff4_load(char **images);

void aff4_seek(AFF4_HANDLE self, uint64_t offset, int whence);
uint64_t aff4_tell(AFF4_HANDLE self);
int aff4_read(AFF4_HANDLE self, char *buf, int len);
void aff4_close(AFF4_HANDLE self);

/** This allows clients to query the resolver for all members with
    specific attributes. If urn,attributes or value are NULL they
    match everything. So for example to find all image objects:

    struct aff4_tripple *image_urns = aff4_query(handle, NULL, AFF4_TYPE,
                                   AFF4_IMAGE);

    returns a null terminated list of tripples. List can be freed with
    aff4_free if desired (or you can free the handle later).
*/
struct aff4_tripple {
  char *urn;
  char *attribute;
  char *value;
};

struct aff4_tripple **aff4_query(AFF4_HANDLE self, char *urn, 
				char *attributes, char *value);

// Make this available even if clients do not use talloc.
void aff4_free(void *ptr);

/** Loads certificate cert and creates a new identify. All segments
    written from now on will be signed using the identity.

    Note that we use the oracle to load the certificate files into
    memory for openssl - this allows the certs to be stored as URNs
    anywhere (on http:// URIs or inside the volume itself).
*/
void add_identity(char *key_file, char *cert);

// This function must be called to initialise the library - we prepare
// all the classes and intantiate an oracle.
void AFF4_Init(void);

#include "exports.h"

#define PACKAGE_VERSION "0.1"

#endif
