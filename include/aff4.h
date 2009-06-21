/** This is the public include file for using libaff4 */
#ifndef __AFF4_H
#define __AFF4_H

#include <stdint.h>

// This is the URI namespace for the AFF4 scheme
#define NAMESPACE "aff4:"
#define VOLATILE_NS "aff4volatile:"
#define FQN "urn:" NAMESPACE
#define FILE_NS "file://"

/** These are standard aff4 attributes */
#define AFF4_STORED     NAMESPACE "stored"
#define AFF4_TYPE       NAMESPACE "type"
#define AFF4_CONTAINS   NAMESPACE "contains"
#define AFF4_SIZE       NAMESPACE "size"
#define AFF4_SHA        NAMESPACE "sha256"
#define AFF4_TIMESTAMP  NAMESPACE "timestamp"

/** ZipFile attributes */
#define AFF4_VOLATILE_HEADER_OFFSET VOLATILE_NS "relative_offset_local_header"
#define AFF4_VOLATILE_COMPRESSED_SIZE VOLATILE_NS "compress_size"
#define AFF4_VOLATILE_CRC VOLATILE_NS "crc32"
#define AFF4_VOLATILE_COMPRESSION VOLATILE_NS "compression"
#define AFF4_VOLATILE_FILE_OFFSET VOLATILE_NS "file_offset"

/** Image attributes */
#define AFF4_CHUNK_SIZE NAMESPACE "chunk_size"
#define AFF4_COMPRESSION NAMESPACE "compression"
#define AFF4_CHUNKS_IN_SEGMENT NAMESPACE "chunks_in_segment"
#define AFF4_DIRECTORY_OFFSET VOLATILE_NS "directory_offset"
#define AFF4_DIRTY       VOLATILE_NS "dirty"

/** Link, encryption attributes */
#define AFF4_TARGET NAMESPACE "target"

/** Map attributes */
#define AFF4_BLOCKSIZE NAMESPACE "blocksize"
#define AFF4_IMAGE_PERIOD NAMESPACE "image_period"
#define AFF4_TARGET_PERIOD NAMESPACE "target_period"

/* Identity attributes */
#define AFF4_STATEMENT NAMESPACE "statement"
#define AFF4_CERT      NAMESPACE "x509"
#define AFF4_PRIV_KEY  VOLATILE_NS "priv_key"
#define AFF4_COMMON_NAME NAMESPACE "common_name"

/** These are standard aff4 types. */
/** Volumes **/
#define AFF4_ZIP_VOLUME       "zip_volume"
#define AFF4_DIRECTORY_VOLUME "directory"
#define AFF4_LIBAFF_VOLUME    "aff1_volume"
#define AFF4_LIBEWF_VOLUME    "ewf_volume"

/** Streams */
#define AFF4_SEGMENT          "segment"
#define AFF4_LINK             "link"
#define AFF4_IMAGE            "image"
#define AFF4_MAP              "map"
#define AFF4_ENCRYTED         "encrypted"
#define AFF4_LIBAFF_STREAM    "aff1_stream"

/** Miscellaneous objects */
#define AFF4_IDENTITY         "identity"
/** This is the configuration namespace - it stores configuration
    information related to the current running instance **/
#define AFF4_CONFIG_NAMESPACE     "urn:aff4config:"
#define AFF4_CONFIG_PAD           "pad_on_read_error"
#define AFF4_CONFIG_AUTOLOAD      "autoload_volumes"

/** These are various properties */
#define AFF4_AUTOLOAD         NAMESPACE "autoload"  /** Instructs the loader to
							load another volume
							related to this one */

/** These are the standard AFF4 interfaces. Interfaces are the basic
    building blocks of an AFF4 volume. So for example image, map and
    encrypted all provide the stream interface and zip_volume and
    directory provide the volume interface.
*/
#define AFF4_INTERFACE        NAMESPACE "interface"
#define AFF4_STREAM           "stream"
#define AFF4_VOLUME           "volume"

  // All identity URNs are prefixed with this:
#define AFF4_IDENTITY_PREFIX  FQN AFF4_IDENTITY

/** Volatile attributes are never written to the file but are used to
    pass messages between different parts of the code.
*/
// Thats the passphrase that will be used to encrypt the session key
#define AFF4_VOLATILE_PASSPHRASE VOLATILE_NS "passphrase"

// This is the session key for encryption
#define AFF4_VOLATILE_KEY   VOLATILE_NS "key"
#define AFF4_CRYPTO_NAMESPACE NAMESPACE "crypto:"
#define AFF4_CRYPTO_FORTIFICATION_COUNT AFF4_CRYPTO_NAMESPACE "fortification"
#define AFF4_CRYPTO_IV  AFF4_CRYPTO_NAMESPACE "IV"
#define AFF4_CRYPTO_PASSPHRASE AFF4_CRYPTO_NAMESPACE "passphrase"
#define AFF4_CRYPTO_ALGORITHM AFF4_CRYPTO_NAMESPACE "algorithm"

// Supported algorithms
#define AFF4_CRYPTO_ALGORITHM_AES_SHA254 "AES256/SHA256"

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
extern int talloc_free(void *ptr);
#define aff4_free(x) talloc_free(x)



/** Loads certificate cert and creates a new identify. All segments
    written from now on will be signed using the identity.

    Note that we use the oracle to load the certificate files into
    memory for openssl - this allows the certs to be stored as URNs
    anywhere (on http:// URIs or inside the volume itself).
*/
void add_identity(char *key_file, char *cert);

#endif
