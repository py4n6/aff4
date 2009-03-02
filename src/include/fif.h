#include "config.h"
#include "class.h"
#include "talloc.h"
#include "zip.h"
#include "stringio.h"

struct FIFFile;

/** Properties are key value pairs. We preserve their order though */
CLASS(Properties, Object)
     struct list_head list;
     char *key;
     char *value;
     uint32_t date;
     
     /** We can create a properties object from a text file which we
	 can parse.
     */
     Properties METHOD(Properties, Con);

     /** Add new key value to the list, returns the value.
     */
     char *METHOD(Properties, add, char *key, char *value, uint32_t date);

     /* An iterator over the properties list to return the list of values
	of the given key.
     */
     char *METHOD(Properties, iter_next, Properties *current, char *key);

     /** Parses the text file and append to our list */
     int METHOD(Properties, parse, char *text, uint32_t len, uint32_t date);

     /** Dumps the current properties array into a string. Callers own the memory. */
     char *METHOD(Properties, str);

     /** Deletes all occurances of values with the specified keys */
     void METHOD(Properties, del, char *key);
END_CLASS

/* A AFFFD is a special FileLikeObject for openning AFF2 streams */
CLASS(AFFFD, FileLikeObject)
// We store properties for the stream here
     Properties props;

     // This is FIFFile who owns.
     struct FIFFile *parent;

     // All Currently open streams are linked here
     struct list_head list;

     // The name of this type
     char *type;

     // The name of this stream (Its UUID)
     char *stream_name;

     AFFFD METHOD(AFFFD, Con, char *stream_name, 
		  Properties props, struct FIFFile *parent);
END_CLASS

// The segments created by the Image stream are named by segment
// number with this format.
#define IMAGE_SEGMENT_NAME_FORMAT "%s/%08d"  /** UUID, segment_id */


/** The Image Stream represents an Image in chunks */
CLASS(Image, AFFFD)
// Data is divided into segments, when a segment is completed (it
// contains chunks_in_segment chunks) we dump it to the archive. These
// are stream attributes
     int chunks_in_segment;   // Default 2048
     int chunk_size;          // Default 32kb   
                              // -> Default segment size 64Mb

     // Writes get queued here until full chunks are available
     StringIO chunk_buffer;

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

END_CLASS

struct dispatch_t {
  char *type;
  AFFFD class_ptr;
};

// A dispatch table for instantiating the right classes according to
// the stream_type
extern struct dispatch_t dispatch[];

// This is the main FIF class - it manages the Zip archive
CLASS(FIFFile, ZipFile)
     Properties props;

     /** This is used to get a new stream handler for writing */
     AFFFD METHOD(FIFFile, create_stream_for_writing, char *stream_name,
		  char *stream_type, Properties props);

     /** Open the stream for reading */
     AFFFD METHOD(FIFFile, open_stream, char *stream_name);
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
  uint64_t file_offset;
  uint64_t image_offset;
  int target_id;
};

CLASS(MapDriver, AFFFD)
// An array of our targets
     AFFFD *targets;
     int number_of_targets;

     // All the points in the map:
     struct map_point *points;
     int number_of_points;
     
     // Deletes the point at the specified file offset
     void METHOD(MapDriver, del, uint64_t file_pos);

     // Adds a new point ot the file offset table
     void METHOD(MapDriver, add, uint64_t file_pos, uint64_t image_offset, 
		 FileLikeObject target);

     void METHOD(MapDriver, save_map);
END_CLASS
