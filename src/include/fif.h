#include "class.h"
#include "talloc.h"
#include "zip.h"

/* A AFFFD is a special FileLikeObject for openning AFF2 streams */
CLASS(AFFFD, FileLikeObject)
     AFFFD METHOD(AFFFD, Con, char *stream_name);
END_CLASS

/** The Image Stream represents an Image in chunks */
CLASS(Image, AFFFD)
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
		  char *stream_type);
END_CLASS

