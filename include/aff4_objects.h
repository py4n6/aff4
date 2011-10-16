#ifndef __AFF4_OBJECTS_H
#define __AFF4_OBJECTS_H

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

  /* Definitions related to the zip volume storage. */
#include "aff4_zip.h"
#include "aff4_image.h"

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

// Some useful utilities
ZipFile open_volume(char *urn);

void zip_init();
void image_init();
void mapdriver_init();
void graph_init();

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif
#endif
