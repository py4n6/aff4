#ifndef __ZIP_H
#define __ZIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "aff4.h"
#include "misc.h"
#include "stringio.h"
#include "list.h"
#include <zlib.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/pem.h>

#define HASH_TABLE_SIZE 256
#define CACHE_SIZE 15

// A helper to access the URN of an object.
#define URNOF(x)  ((AFFObject)x)->urn

/** Some helper functions used to serialize int to and from URN
    attributes
*/
uint64_t parse_int(char *string);
char *from_int(uint64_t arg);
char *escape_filename(const char *filename);
char *unescape_filename(const char *filename);

/** A cache is an object which automatically expires data which is
    least used - that is the data which is most used is put at the end
    of the list, and when memory pressure increases we expire data
    from the front of the list.
*/
enum Cache_policy {
  CACHE_EXPIRE_FIRST,
  CACHE_EXPIRE_LEAST_USED
};

CLASS(Cache, Object)
// The key which is used to access the data
     void *key;

     // An opaque data object and its length. The object will be
     // talloc_stealed into the cache object as we will be manging its
     // memory.
     void *data;
     int data_len;

     // Cache objects are put into two lists - the cache_list contains
     // all the cache objects currently managed by us in order of
     // least used to most used at the tail of the list. The same
     // objects are also present on one of the hash lists which hang
     // off the respective hash table. The hash_list should be shorter
     // to search linearly as it only contains objects with the same hash.
     struct list_head cache_list;
     struct list_head hash_list;

     // This is a pointer to the head of the cache
     struct Cache_t *cache_head;
     enum Cache_policy policy;

     // The current number of objects managed by this cache
     int cache_size;

     // The maximum number of objects which should be managed
     int max_cache_size;

     // A hash table of the keys
     int hash_table_width;
     Cache *hash_table;

     // These functions can be tuned to manage the hash table. The
     // default implementation assumes key is a null terminated
     // string.
     unsigned int METHOD(Cache, hash, void *key);
     int METHOD(Cache, cmp, void *other);

     Cache METHOD(Cache, Con, int hash_table_width, int max_cache_size);

// Return a cache object or NULL if its not there. Callers do not own
// the cache object. If they want to steal the data, they can but they
// must call talloc_free on the Cache object so it can be removed from
// the cache.
// (i.e. talloc_steal(result->data); talloc_free(result); )
     Cache METHOD(Cache, get, void *key);

// A shorthand for getting the actual data itself rather than the
// Cache object itself:
     void *METHOD(Cache, get_item, char *key);

// Store the key, data in a new Cache object. The key and data will be
// stolen.
     Cache METHOD(Cache, put, void *key, void *data, int data_len);
END_CLASS

/** All AFF Objects inherit from this one. The URI must be set to
    represent the globally unique URI of this object. */
CLASS(AFFObject, Object)
     char *urn;

     // This is the type of this object
     char *type;

     // Is this object a reader or a writer?
     char mode;
     
     /** Any object may be asked to be constructed from its URI */
     AFFObject METHOD(AFFObject, Con, char *uri, char mode);

     /** This is called to set properties on the object */
     void METHOD(AFFObject, set_property, char *attribute, char *value);

     /** Finally the object may be ready for use. We return the ready
	 object or NULL if something went wrong.
     */
     AFFObject METHOD(AFFObject, finish);

/** This is how an AFFObject can be created. First the oracle is asked
    to create new instance of that object:

    FileLikeObject fd = CALL(oracle, create, CLASSOF(FileLikeObject));

    Now properties can be set on the object:
    CALL(fd, set_property, "aff2:location", "file://hello.txt")

    Finally we make the object ready for use:
    CALL(fd, finish)

    and CALL(fd, write, ZSTRING_NO_NULL("foobar"))
*/

END_CLASS

// Base class for file like objects
#define MAX_CACHED_FILESIZE 1e6

CLASS(FileLikeObject, AFFObject)
     int64_t readptr;
     uint64_t size;
     char mode;
     char *data;

     uint64_t METHOD(FileLikeObject, seek, int64_t offset, int whence);
     int METHOD(FileLikeObject, read, char *buffer, unsigned long int length);
     int METHOD(FileLikeObject, write, char *buffer, unsigned long int length);
     uint64_t METHOD(FileLikeObject, tell);
  
  // This can be used to get the content of the FileLikeObject in a
  // big buffer of data. The data will be cached with the
  // FileLikeObject. Its only really suitable for smallish amounts of
  // data - and checks to ensure that file size is less than MAX_CACHED_FILESIZE
     char *METHOD(FileLikeObject, get_data);

// This method is just like the standard ftruncate call
     int METHOD(FileLikeObject, truncate, uint64_t offset);

// This closes the FileLikeObject and also frees it - it is not valid
// to use the FileLikeObject after calling this (it gets free'd).
     void METHOD(FileLikeObject, close);
END_CLASS

// This file like object is backed by a real disk file:
CLASS(FileBackedObject, FileLikeObject)
     int fd;
     FileBackedObject METHOD(FileBackedObject, Con, char *filename, char mode);
END_CLASS

#ifdef HAVE_LIBCURL
#include <curl/curl.h>

// This is implemented in using libcurl
CLASS(HTTPObject, FileLikeObject)
// The socket
     CURL *curl;
     StringIO buffer;

     CURL *send_handle;
     StringIO send_buffer;
     int send_buffer_offset;

     CURLM *multi_handle;

     HTTPObject METHOD(HTTPObject, Con, char *url);
END_CLASS
#endif

     /** The resolver is at the heart of the AFF2 specification - its
	 responsible with returning various objects from a globally
	 unique identifier (URI).
     */
CLASS(Resolver, AFFObject)
// This is a global cache of URN and their values - we try to only
// have small URNs here and keep everything in memory.
     Cache urn;

     // This is a cache of AFFObject objects (keyed by URI) which have
     // been constructed previously. This cache is quite small and can
     // expire objects at any time. All clients of the open() method
     // get references back from this cache. They do not own any of
     // the objects and must not maintain references to them. Clients
     // may keep references to each object's urn and re-fetch the
     // object from the open method each time they want to use
     // it. This implies that clients do not need to generally do any
     // caching at all.
     Cache cache;

     // This is a cache of writers
     Cache writers;

     // Resolvers contain the identity behind them (see below):
     struct Identity_t *identity;

     Resolver METHOD(Resolver, Con);

     // Resolvers are all in a list. Each resolver in the list is another
     // identity which can be signed.
     struct list_head identities;

/* This method tries to resolve the provided uri and returns an
 instance of whatever the URI refers to (As an AFFObject which is the
 common base class. You should check to see that what you get back is
 actually what you need. For example:

 FileLikeObject fd = (FileLikeObject)CALL(resolver, resolve, uri);
 if(!fd || !ISSUBCLASS(fd, FileLikeObject)) goto error;
 
 Once the resolver provides the object it is attached to the context
 ctx and removed from the cache. This ensures that the object can not
 expire from the cache while callers are holding it. For efficiency
 you must return the object to the cache as soon as possible by
 calling cache_return.
*/ 
      AFFObject METHOD(Resolver, open, void *ctx, char *uri, char mode);
      void METHOD(Resolver, cache_return, AFFObject obj);

/* This create a new object of the specified type. */
     AFFObject METHOD(Resolver, create, AFFObject *class_reference);

/* Returns an attribute about a particular uri if known. This may
     consult an external data source.
*/
     char *METHOD(Resolver, resolve, char *uri, char *attribute);

/** This returns a null terminated list of matches. */
     char **METHOD(Resolver, resolve_list, void *ctx, char *uri, char *attribute);

//Stores the uri and the value in the resolver. The value and uri will
//be stolen.
     void METHOD(Resolver, add, char *uri, char *attribute, char *value);

     // Exports all the properties to do with uri - user owns the
     // buffer. context is the URN which will ultimately hold the
     // exported file. If uri is the same as context, we write the
     // statement as a relative notation.
     char *METHOD(Resolver, export_urn, char *uri, char *context);

     // Exports all the properties to do with uri - user owns the buffer.
     char *METHOD(Resolver, export_all, char *context);

// Deletes the attribute from the resolver
     void METHOD(Resolver, del, char *uri, char *attribute);

// This updates the value or adds it if needed
     void METHOD(Resolver, set, char *uri, char *attribute, char *value);

  //This returns 1 if the statement is set
     int METHOD(Resolver, is_set, char *uri, char *attribute, char *value);

     // Parses the properties file
     void METHOD(Resolver, parse, char *context, char *text, int len);
END_CLASS

// This is a global instance of the oracle. All AFFObjects must
// communicate with the oracle rather than instantiate their own.
extern Resolver oracle;

/** This object represents an identify - like a person for example. 

An identity is someone who makes statements about other AFFObjects in
the universe.
*/
CLASS(Identity, AFFObject)
       Resolver info;
       
       EVP_PKEY *priv_key;
       EVP_PKEY *pub_key;
       X509 *x509;

       Identity METHOD(Identity, Con, char *cert, char *priv_key, char mode);
       void METHOD(Identity, store, char *volume_urn);
  /** This method asks the identity to verify its statements. This
       essentially populates our Resolver with statements which can be
       verified from our statements. Our Resolver can then be
       compared to the oracle to see which objects do not match.
  */
       void METHOD(Identity, verify, int (*cb)(uint64_t progress, char *urn));
END_CLASS

// This function must be called to initialise the library - we prepare
// all the classes and intantiate an oracle.
void AFF2_Init(void);

// A link simply returns the URI its pointing to
CLASS(Link, AFFObject)
     void METHOD(Link, link, Resolver resolver, char *storage_urn, 
		 char *target, char *friendly_name);
END_CLASS

/** The Image Stream represents an Image in chunks */
CLASS(Image, FileLikeObject)
// Data is divided into segments, when a segment is completed (it
// contains chunks_in_segment chunks) we dump it to the archive. These
// are stream attributes
     int chunks_in_segment;   // Default 2048
     int chunk_size;          // Default 32kb   
                              // -> Default segment size 64Mb

     // Writes get queued here until full chunks are available
     char *chunk_buffer;
     int chunk_buffer_readptr;
     int chunk_buffer_size;

     // The segment is written here until its complete and then it
     // gets flushed
     StringIO segment_buffer;
     int chunk_count;

     // Chunks are cached here. We cant use the main zip file cache
     // because the zip file holds the full segment
     Cache chunk_cache;

     int segment_count;

     // An array of indexes into the segment where chunks are stored
     int32_t *chunk_indexes;
     char *parent_urn;

     // This is the compression type
     int compression;

     EVP_MD_CTX digest;
END_CLASS

/** The map stream driver maps an existing stream using a
    transformation.


    We require the stream properties to specify a 'target'. This can
    either be a plain stream name or can begin with 'file://'. In the
    latter case this indicates that we should be opening an external
    file of the specified filename.

    We expect to find a component in the archive called 'map' which
    contains a mapping function. The file should be of the format:

    - lines starting with # are ignored
    
    - other lines have 2 integers seperated by white space. The first
    column is the current stream offset, while the second offset if
    the target stream offset.

    For example:
    0     1000
    1000  4000

    This means that when the current stream is accessed in the range
    0-1000 we fetch bytes 1000-2000 from the target stream, and after
    that we fetch bytes from offset 4000.

    Required properties:
    
    - target%d starts with 0 the number of target (may be specified as
      a URL). e.g. target0, target1, target2

    Optional properties:

    - file_period - number of bytes in the file offset which this map
      repreats on. (Useful for RAID)

    - image_period - number of bytes in the target image each period
      will advance by. (Useful for RAID)
*/
struct map_point {
  // The offset in the target
  uint64_t target_offset;
  // This logical offset this represents
  uint64_t image_offset;
  char *target_urn;
};

CLASS(MapDriver, FileLikeObject)
// An array of our targets
     struct map_point *points;
     int number_of_points;
     
     // The period offsets repeat within each target
     uint64_t target_period;
     // The period offsets repear within the logical image
     uint64_t image_period;
     
     char *parent_urn;

     // Deletes the point at the specified file offset
     void METHOD(MapDriver, del, uint64_t target_pos);

     // Adds a new point ot the file offset table
     void METHOD(MapDriver, add, uint64_t image_offset, uint64_t target_offset,
		 char *target);

     void METHOD(MapDriver, save_map);
END_CLASS

/************************************************************
  An implementation of the encrypted Stream.

  This stream encrypts and decrypts data from a target stream in
  blocks determined by the "block_size" attribute. The IV consists of
  the block number (as 32 bit int in little endian) appended to an 8
  byte salt.
*************************************************************/
CLASS(Encrypted, FileLikeObject)
     StringIO block_buffer;
     char *key;
     char *salt;
     char *target_urn;
     // Our volume urn
     char *volume;

     // The block size for CBC mode
     int block_size;
END_CLASS

 char *resolver_get_with_default(Resolver self, char *urn, 
				 char *attribute, char *default_value);

  // This is the largest file size which may be represented by a
  // regular zip file without using Zip64 extensions.
#define ZIP64_LIMIT ((1LL << 31)-1)
  //#define ZIP64_LIMIT 1

/** These are ZipFile structures */
struct EndCentralDirectory {
  uint32_t magic;
  uint16_t number_of_this_disk;
  uint16_t disk_with_cd;
  uint16_t total_entries_in_cd_on_disk;
  uint16_t total_entries_in_cd;
  uint32_t size_of_cd;
  uint32_t offset_of_cd;
  uint16_t comment_len;
}__attribute__((packed));

/** As we parse these fields we populate the oracle */
struct CDFileHeader {
  uint32_t magic;
  uint16_t version_made_by;
  uint16_t version_needed;
  uint16_t flags;
  uint16_t compression_method;	/* aff2volatile:compression */
  uint16_t dostime;		/* aff2volatile:timestamp */
  uint16_t dosdate;
  uint32_t crc32;
  uint32_t compress_size;	/* aff2volatile:compress_size */
  uint32_t file_size;		/* aff2volatile:file_size */
  uint16_t file_name_length;
  uint16_t extra_field_len;
  uint16_t file_comment_length;
  uint16_t disk_number_start;
  uint16_t internal_file_attr;
  uint32_t external_file_attr;
  uint32_t relative_offset_local_header; /* aff2volatile:header_offset */
}__attribute__((packed));

struct ZipFileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t flags;
  uint16_t compression_method;
  uint16_t lastmodtime;
  uint16_t lastmoddate;
  uint32_t crc32;
  uint32_t compress_size;
  uint32_t file_size;
  uint16_t file_name_length;
  uint16_t extra_field_len;
}__attribute__((packed));

struct Zip64EndCD {
  uint32_t magic;
  uint64_t size_of_header;
  uint16_t version_made_by;
  uint16_t version_needed;
  uint32_t number_of_disk;
  uint32_t number_of_disk_with_cd;
  uint64_t number_of_entries_in_volume;
  uint64_t number_of_entries_in_total;
  uint64_t size_of_cd;
  uint64_t offset_of_cd;
}__attribute__((packed));

struct Zip64CDLocator {
  uint32_t magic;
  uint32_t disk_with_cd;
  uint64_t offset_of_end_cd;
  uint32_t number_of_disks;
}__attribute__((packed));

/** This represents a Zip file */
CLASS(ZipFile, AFFObject)
     // This keeps the end of central directory struct so we can
     // recopy it when we update the CD.
     struct EndCentralDirectory *end;

     // The following are attributes stored by various zip64 extended
     // fields
     uint64_t total_entries;
     uint64_t original_member_size;
     uint64_t compressed_member_size;
     uint64_t offset_of_member_header;
     uint64_t directory_offset;

     // This is our own current URN - new files will be appended to
     // that
     char *parent_urn;

     /** A zip file is opened on a file like object */
     ZipFile METHOD(ZipFile, Con, char *file_urn, char mode);

// Fetch a member as a string - this is suitable for small memebrs
// only as we allocate memory for it. The buffer callers receive will
// owned by ctx. 
     char *METHOD(ZipFile, read_member, void *ctx,
		  char *filename, int *len);

// This method is called to specify a new volume for us to use. If we
// already have an existing volume, we will automatically call close
// on it. The volume parent_urn must already exist (it can be newly
// created).
     int METHOD(ZipFile, create_new_volume, char *parent_urn);

// This method opens an existing member or creates a new one. We
// return a file like object which may be used to read and write the
// member. If we open a member for writing the zip file will be locked
// (so another attempt to open a new member for writing will raise,
// until this member is promptly closed). The ZipFile must have been
// called with create_new_volume or append_volume before.
     FileLikeObject METHOD(ZipFile, open_member, char *filename, char mode,
			   char *extra, uint16_t extra_field_len,
			   int compression);

// This method flushes the central directory and finalises the
// file. The file may still be accessed for reading after this.
     void METHOD(ZipFile, close);

// A convenience function for storing a string as a new file (it
// basically calls open_member, writes the string then closes it).
     int METHOD(ZipFile, writestr, char *filename, char *data, int len,
		 char *extra, int extra_field_len,
		 int compression);
END_CLASS

#define ZIP_STORED 0
#define ZIP_DEFLATE 8

// This is a FileLikeObject which is used from within the Zip file:
CLASS(ZipFileStream, FileLikeObject)
     z_stream strm;
     uint64_t file_offset;
     char *container_urn;
     char *parent_urn;
     uint32_t crc32;
     uint32_t compress_size;
     uint32_t compression;

     // We calculate the SHA256 hash of each archive member
     EVP_MD_CTX digest;

// This is the constructor for the file like object. Note that we
// steal the underlying file pointer which should be the underlying
// zip file and should be given to us already seeked to the right
// place.
     ZipFileStream METHOD(ZipFileStream, Con, char *filename, 
			  char *parent_urn, char *container_urn,
			  char mode);
END_CLASS

// A directory volume implementation - all elements live in a single
// directory structure
CLASS(DirVolume, ZipFile)
END_CLASS

void dump_stream_properties(FileLikeObject self, char *volume);

//Some useful utilities
ZipFile open_volume(char *urn, char mode);

  // Members in a volume may have a URN relative to the volume
  // URN. This is still globally unique, but may be stored in a more
  // concise form. For example the ZipFile member "default" is a
  // relative name with a fully qualified name of
  // Volume_URN/default. These functions are used to convert from
  // fully qualified to relative names as needed. The result is a
  // static buffer.
char *fully_qualified_name(char *name, char *volume_urn);
char *relative_name(char *name, char *volume_urn);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif

