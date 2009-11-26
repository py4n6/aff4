/** This file defines constants used in AFF4. These constants are
    required to ensure that AFF4 files are compatible between
    implementations. We therefore keep this file seperate and refer
    to the constants defined here everywhere else.
*/

// This is the URI namespace for the AFF4 scheme
#define NAMESPACE "aff4://"
#define PREDICATE_NAMESPACE "http://afflib.org/2009/aff4/#"
#define RDF_NAMESPACE "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define XSD_NAMESPACE "http://www.w3.org/2001/XMLSchema#"

#define VOLATILE_NS "aff4volatile:"
#define CONFIGURATION_NS VOLATILE_NS "config:"
#define FQN NAMESPACE

// This object holds parameters pertinent to the running instance
#define GLOBAL VOLATILE_NS "global"

// Configuration parameters
#define CONFIG_THREADS  CONFIGURATION_NS  "threads"
#define CONFIG_VERBOSE  CONFIGURATION_NS  "verbosity"
#define CONFIG_AUTOLOAD  CONFIGURATION_NS "autoload"
#define CONFIG_PAD       CONFIGURATION_NS "pad"

/** These are standard aff4 attributes */
#define AFF4_STORED     PREDICATE_NAMESPACE "stored"
#define AFF4_TYPE       RDF_NAMESPACE "type"
#define AFF4_INTERFACE  PREDICATE_NAMESPACE "interface"
#define AFF4_SIZE       PREDICATE_NAMESPACE "size"
#define AFF4_SHA        PREDICATE_NAMESPACE "sha256"
#define AFF4_TIMESTAMP  PREDICATE_NAMESPACE "timestamp"

// Supported interfaces
#define AFF4_STREAM     PREDICATE_NAMESPACE "stream"
#define AFF4_VOLUME     PREDICATE_NAMESPACE "volume"

/** ZipFile attributes */
#define AFF4_VOLATILE_HEADER_OFFSET VOLATILE_NS "relative_offset_local_header"
#define AFF4_VOLATILE_COMPRESSED_SIZE VOLATILE_NS "compress_size"
#define AFF4_VOLATILE_CRC VOLATILE_NS "crc32"
#define AFF4_VOLATILE_COMPRESSION VOLATILE_NS "compression"
#define AFF4_VOLATILE_FILE_OFFSET VOLATILE_NS "file_offset"
#define AFF4_VOLATILE_DIRTY VOLATILE_NS "dirty"
#define AFF4_VOLATILE_CONTAINS   VOLATILE_NS "contains"
//Its useful for debugging to enable this so the RDF file also shows
//the contains property for a volume. It is not needed normally since
//its infered from the zip file itself.
//#define AFF4_VOLATILE_CONTAINS   PREDICATE_NAMESPACE "contains"

// Volume attributes
#define AFF4_IDENTITY_STORED PREDICATE_NAMESPACE "identity" /* Indicates an identity
						      is stored in this
						      volume */

#define AFF4_AUTOLOAD PREDICATE_NAMESPACE "autoload" /* A hint that this stream
					      should be automatically
					      loaded as a volume */

/** Image attributes */
#define AFF4_CHUNK_SIZE PREDICATE_NAMESPACE "chunk_size"
#define AFF4_COMPRESSION PREDICATE_NAMESPACE "compression"
#define AFF4_CHUNKS_IN_SEGMENT PREDICATE_NAMESPACE "chunks_in_segment"
#define AFF4_DIRECTORY_OFFSET VOLATILE_NS "directory_offset"

/** Link, encryption attributes */
#define AFF4_TARGET PREDICATE_NAMESPACE "target"

/** Map attributes */
#define AFF4_BLOCKSIZE PREDICATE_NAMESPACE "blocksize"
#define AFF4_IMAGE_PERIOD PREDICATE_NAMESPACE "image_period"
#define AFF4_TARGET_PERIOD PREDICATE_NAMESPACE "target_period"

/* Identity attributes */
#define AFF4_STATEMENT PREDICATE_NAMESPACE "statement"
#define AFF4_CERT      PREDICATE_NAMESPACE "x509"
#define AFF4_PRIV_KEY  VOLATILE_NS "priv_key"
#define AFF4_COMMON_NAME PREDICATE_NAMESPACE "common_name"
#define AFF4_IDENTITY_PREFIX  FQN  "identity"
#define AFF4_HASH_TYPE        FQN  "hash_type"

// A property indicating this object should be highlighted
#define AFF4_HIGHLIGHT        PREDICATE_NAMESPACE   "highlight"  

// Information segments are named starting with this (and ending with
// the RDF encoding):
#define AFF4_INFORMATION     "information."

// Encrypted stream attributes
// Thats the passphrase that will be used to encrypt the session key
#define AFF4_VOLATILE_PASSPHRASE VOLATILE_NS "passphrase"

// This is the master key for encryption (Never written down)
#define AFF4_VOLATILE_KEY               VOLATILE_NS "key"
#define AFF4_CRYPTO_NAMESPACE           NAMESPACE "crypto:"

/* The intermediate key is obtained from pbkdf2() of the
   passphrase and salt. Iteration count is the fortification.*/
#define AFF4_CRYPTO_FORTIFICATION_COUNT AFF4_CRYPTO_NAMESPACE "fortification"
#define AFF4_CRYPTO_IV       AFF4_CRYPTO_NAMESPACE "iv"
#define AFF4_CRYPTO_RSA      AFF4_CRYPTO_NAMESPACE "rsa"

// This is the image master key encrypted using the intermediate key
#define AFF4_CRYPTO_PASSPHRASE_KEY      AFF4_CRYPTO_NAMESPACE "passphrase_key"
#define AFF4_CRYPTO_ALGORITHM           AFF4_CRYPTO_NAMESPACE "algorithm"
#define AFF4_CRYPTO_BLOCKSIZE           AFF4_CRYPTO_NAMESPACE "blocksize"
/* The nonce is the salt encrypted using the image master key. Its
   used to check the master key is correct: */
#define AFF4_CRYPTO_NONCE               AFF4_CRYPTO_NAMESPACE "nonce"

// Supported algorithms
#define AFF4_CRYPTO_ALGORITHM_AES_SHA254 "AES256/SHA256"

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
#define AFF4_ERROR_STREAM     "error"

/** Miscellaneous objects */
#define AFF4_IDENTITY         "identity"

/* The following URNs are special and should be known by the
   implementation: */
#define AFF4_SPECIAL_URN_NULL FQN "null" /* This URN refers to NULL data
					    in Sparse maps (unread data
					    not the same as zero) */

#define AFF4_SPECIAL_URN_ZERO FQN "zero" /* This is used to represent long
					    runs of zero */

