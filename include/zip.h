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
#include <pthread.h>
#include <tdb.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

// A link simply returns the URI its pointing to
CLASS(Link, AFFObject)
     void METHOD(Link, link, Resolver resolver, char *storage_urn, 
		 char *target, char *friendly_name);
END_CLASS

#include "queue.h"

CLASS(ImageWorker, AFFObject)
       struct list_head list;
  // The filename where the volume is stored - we fetch that just
  // prior to writing it
       RDFURN stored;

       int segment_count;

       // This is the queue which this worker belongs in
       Queue queue;

       // The bevy is written here in its entirety. When its finished,
       // we compress it all and dump it to the output file.
       StringIO bevy;
       
       // The segment is written here until it is complete and then it
       // gets flushed
       StringIO segment_buffer;

       // When we finish writing a bevy, a thread is launched to
       // compress and dump it
       pthread_t thread;
       struct Image_t *image;

       // An array of indexes into the segment where chunks are stored
       int32_t *chunk_indexes;

       ImageWorker METHOD(ImageWorker, Con, struct Image_t *image);

       // A write method for the worker
       int METHOD(ImageWorker, write, char *buffer, int len);
END_CLASS

/** The Image Stream represents an Image in chunks */
CLASS(Image, FileLikeObject)
     // This is where the image is stored
     RDFURN stored;

  // These are the URNs for the bevy and the bevy index
  RDFURN bevy_urn;

     // Chunks are cached here. We cant use the main zip file cache
     // because the zip file holds the full segment
     Cache chunk_cache;

     // This is a queue of all workers available
     Queue workers;

     // This is a queue of all workers busy
     Queue busy;

     // Thats the current worker we are using - when it gets full, we
     // simply dump its bevy and take a new worker here.
     ImageWorker current;

     XSDInteger chunk_size;
     XSDInteger compression;
     XSDInteger chunks_in_segment;
     uint32_t bevy_size;

     int segment_count;

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
     XSDInteger target_period;

     // The period offsets repear within the logical image
     XSDInteger image_period;

     // The blocksize is a constant multiple for each map offset.
     XSDInteger blocksize;
   
     // This where we get stored
     RDFURN stored;
     RDFURN map_urn;
     RDFURN target;

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
#include <openssl/aes.h>

CLASS(Encrypted, FileLikeObject)
     StringIO block_buffer;
     AES_KEY ekey;
     AES_KEY dkey;
     char *salt;
     char *target_urn;
     // Our volume urn
     char *volume;

     // The block size for CBC mode
     int block_size;
     int block_number;
END_CLASS

void *resolver_get_with_default(Resolver self, char *urn, 
				char *attribute, void *default_value,
				enum resolver_data_type type);

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

  /** Some commonly used RDF types */
  XSDInteger directory_offset;
  RDFURN storage_urn;
  XSDInteger _didModify;

  /** attributes of files used for the EndCentralDirectory */
  XSDInteger type;
  XSDInteger epoch_time;
  XSDInteger compression_method;
  XSDInteger crc;
  XSDInteger size;
  XSDInteger compressed_size;
  XSDInteger header_offset;

     /** A zip file is opened on a file like object */
     ZipFile METHOD(ZipFile, Con, char *file_urn, char mode);

// This method opens an existing member or creates a new one. We
// return a file like object which may be used to read and write the
// member. If we open a member for writing the zip file will be locked
// (so another attempt to open a new member for writing will raise,
// until this member is promptly closed). The ZipFile must have been
// called with create_new_volume or append_volume before.
     FileLikeObject METHOD(ZipFile, open_member, char *filename, char mode,
			   uint16_t compression);

// This method flushes the central directory and finalises the
// file. The file may still be accessed for reading after this.
     void METHOD(ZipFile, close);

// A convenience function for storing a string as a new file (it
// basically calls open_member, writes the string then closes it).
     int METHOD(ZipFile, writestr, char *filename, char *data, int len,
		 uint16_t compression);

     int METHOD(ZipFile, load_from, RDFURN fd_urn, char mode);
END_CLASS

#define ZIP_STORED 0
#define ZIP_DEFLATE 8

// A directory volume implementation - all elements live in a single
// directory structure
CLASS(DirVolume, ZipFile)
END_CLASS

#ifdef HAVE_LIBAFFLIB
#include "aff1.h"
#endif

#ifdef HAVE_LIBEWF
#include "ewfvolume.h"
#endif

/** This is a dispatcher of stream classes depending on their name.
*/
struct dispatch_t {
  // A boolean to determine if this is a scheme or a type
  int scheme;
  char *type;
  AFFObject class_ptr;
};

extern struct dispatch_t dispatch[];
extern struct dispatch_t volume_handlers[];

void dump_stream_properties(FileLikeObject self);

//Some useful utilities
ZipFile open_volume(char *urn, char mode);

  // Members in a volume may have a URN relative to the volume
  // URN. This is still globally unique, but may be stored in a more
  // concise form. For example the ZipFile member "default" is a
  // relative name with a fully qualified name of
  // Volume_URN/default. These functions are used to convert from
  // fully qualified to relative names as needed. The result is a
  // static buffer.
  //char *fully_qualified_name(void *ctx, char *name, RDFURN volume_urn);
  //char *relative_name(void *ctx, char *name, RDFURN volume_urn);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif

