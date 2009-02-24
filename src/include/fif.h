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
     int METHOD(Properties, parse, char *text, uint32_t date);

     /** Dumps the current properties array into a string. Callers own the memory. */
     char *METHOD(Properties, str);
END_CLASS

/* A AFFFD is a special FileLikeObject for openning AFF2 streams */
CLASS(AFFFD, FileLikeObject)
// We store properties for the stream here
     Properties props;

     // This is FIFFile who owns.
     struct FIFFile *parent;
  
     AFFFD METHOD(AFFFD, Con, char *stream_name, 
		  Properties props, struct FIFFile *parent);
END_CLASS

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

     int segment_count;

     // An array of indexes into the segment where chunks are stored
     uint32_t *chunk_indexes;

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

     AFFFD METHOD(FIFFile, create_stream_for_writing, char *stream_name,
		  char *stream_type, Properties props);
END_CLASS

