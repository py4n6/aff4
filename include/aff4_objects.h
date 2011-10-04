#ifndef __ZIP_H
#define __ZIP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "aff4_internal.h"
#include <zlib.h>
#include "queue.h"
#include "trp.h"

/** A Graph is a named collection of RDF statements about various
    objects.

    When the graph is stored it will serialise into a segment with all
    the statements contained with it using the appropriate RDF
    serialization.
*/
CLASS(Graph, AFFObject)
  RDFURN stored;
  RDFURN attribute_urn;
  XSDString statement;

  /** Add a new triple to this Graph */
  void METHOD(Graph, set_triple, RDFURN subject, char *attribute, RDFValue value);
END_CLASS

/* This is the worker object itself (private) */
struct ImageWorker_t;

/** The Image Stream represents an Image in chunks */
CLASS(Image, FileLikeObject)
/* This is where the image is stored */
  RDFURN stored;

  /* These are the URNs for the bevy and the bevy index */
  RDFURN bevy_urn;

  /* Chunks are cached here for faster randoom reading performance.
  */
  Cache chunk_cache;

  /* This is a queue of all workers available */
  Queue workers;

  /* This is a queue of all workers busy */
  Queue busy;

  /* Thats the current worker we are using - when it gets full, we
     simply dump its bevy and take a new worker here.
  */
  struct ImageWorker_t *current;

  /** Some parameters about this image */
  XSDInteger chunk_size;
  XSDInteger compression;
  XSDInteger chunks_in_segment;
  uint32_t bevy_size;

  int segment_count;

  EVP_MD_CTX digest;

  /** This sets the number of working threads.

      The default number of threads is zero (no threads). Set to a
      higher number to utilize multiple threads here.
  */
  void METHOD(Image, set_workers, int workers);
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

    Optional properties:

    - file_period - number of bytes in the file offset which this map
      repreats on. (Useful for RAID)

    - image_period - number of bytes in the target image each period
      will advance by. (Useful for RAID)
*/
struct map_point_tree_s;

CLASS(MapValue, RDFValue)
  // The urn of the map object we belong to
  RDFURN urn;
  XSDInteger size;

  int number_of_points;
  int number_of_urns;

  // This is where we store the node treap
  struct map_point_tree_s *tree;

  // An array of all the targets we know about
  RDFURN *targets;
  Cache cache;

  // The period offsets repeat within each target
  XSDInteger target_period;

  // The period offsets repear within the logical image
  XSDInteger image_period;

  // The blocksize is a constant multiple for each map offset.
  XSDInteger blocksize;

  uint64_t METHOD(MapValue, add_point, uint64_t image_offset, uint64_t target_offset, \
                  char *target);

  /* This function returns information about the current file pointer
     and its view of the target slice.

     DEFAULT(target_idx) = NULL;
  */
  BORROWED RDFURN METHOD(MapValue, get_range, uint64_t readptr,         \
                         OUT uint64_t *target_offset_at_point,          \
                         OUT uint64_t *available_to_read,               \
                         OUT uint32_t *target_idx                       \
                         );
END_CLASS

// Some alternative implementations
CLASS(MapValueBinary, MapValue)
END_CLASS

// Sometimes its quicker to just serialise the map inline
CLASS(MapValueInline, MapValue)
  char *buffer;
  int i;
END_CLASS

CLASS(MapDriver, FileLikeObject)
     MapValue map;

     // This where we get stored
     RDFURN stored;

     RDFURN target_urn;
     XSDInteger dirty;

     // This flag indicates if we should automatically set the most
     // optimal map implementation
     int custom_map;

  // Sets the data type of the map object
     void METHOD(MapDriver, set_data_type, char *type);

     // Deletes the point at the specified file offset
     void METHOD(MapDriver, del, uint64_t target_pos);

     // Adds a new point to the file offset table
     void METHOD(MapDriver, add_point, uint64_t image_offset, uint64_t target_offset,\
		 char *target);

  /* This interface is a more natural way to build the map, by
     simulating copying from different offsets in different targets
     sequentially. This function will add a new map point, and advance
     the readptr by the specified length.
  */
     void METHOD(MapDriver, write_from, RDFURN target, uint64_t target_offset,\
                 uint64_t length);

     void METHOD(MapDriver, save_map);
END_CLASS

PROXY_CLASS(MapDriver);
PROXY_CLASS(Image);

#ifdef HAVE_OPENSSL
#include "aff4_crypto.h"
#endif

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
CLASS(ZipFile, AFF4Volume)
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
END_CLASS

// This is a FileLikeObject which is used to provide access to zip
// archive members. Currently only accessible through
// ZipFile.open_member()
CLASS(ZipFileStream, FileLikeObject)
     z_stream strm;
     XSDInteger file_offset;
     // The file backing the container
     RDFURN file_urn;

     // The container ZIP file we are written in
     RDFURN container_urn;
     FileLikeObject file_fd;
     XSDInteger crc32;
     XSDInteger compress_size;
     XSDInteger compression;
     XSDInteger dirty;

     // For now we just decompress the entire segment into memory if
     // required
     char *cbuff;
     char *buff;

     // We calculate the SHA256 hash of each archive member
     EVP_MD_CTX digest;

     /* This is the constructor for the file like object. 
	file_urn is the storage file for the volume in
	container_urn. If the stream is opened for writing the file_fd
	may be passed in. It remains locked until we are closed.
     */
     ZipFileStream METHOD(ZipFileStream, Con, RDFURN urn,                  \
                          RDFURN file_urn, RDFURN container_urn,        \
			  char mode, FileLikeObject file_fd);
END_CLASS

#define ZIP_STORED 0
#define ZIP_DEFLATE 8

// A directory volume implementation - all elements live in a single
// directory structure
PRIVATE CLASS(DirVolume, ZipFile)
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
ZipFile open_volume(char *urn);

  // Members in a volume may have a URN relative to the volume
  // URN. This is still globally unique, but may be stored in a more
  // concise form. For example the ZipFile member "default" is a
  // relative name with a fully qualified name of
  // Volume_URN/default. These functions are used to convert from
  // fully qualified to relative names as needed. The result is a
  // static buffer.
  //char *fully_qualified_name(void *ctx, char *name, RDFURN volume_urn);
  //char *relative_name(void *ctx, char *name, RDFURN volume_urn);

void zip_init();
void image_init();
void mapdriver_init();
void graph_init();

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif
