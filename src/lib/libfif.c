#include "fif.h"

/** This is a dispatches of stream classes depending on their name */
struct dispatch_t dispatch[] = {
  { "Image", (AFFFD)(&__Image) },
  { NULL, NULL}
};

/** We need to call these initialisers explicitely or the class
    references wont work.
*/
static void init_streams(void) {
  FileLikeObject_init();
  AFFFD_init();
  Image_init();
};

AFFFD FIFFile_create_stream_for_writing(FIFFile self, char *stream_name,
					char *stream_type) {
  int i;

  init_streams();

  for(i=0; dispatch[i].type !=NULL; i++) {
    if(!strcmp(dispatch[i].type, stream_type)) {
      AFFFD result;

      // A special constructor from a class reference
      result = CONSTRUCT_FROM_REFERENCE(dispatch[i].class_ptr, 
					Con, self, stream_name);
      return result;
    };
  };

  RaiseError(ERuntimeError, "No handler for stream_type '%s'", stream_type);
  return NULL;
};

VIRTUAL(FIFFile, ZipFile)
     VMETHOD(create_stream_for_writing) = FIFFile_create_stream_for_writing;
END_VIRTUAL

AFFFD AFFFD_Con(AFFFD self, char *stream_name) {
  return self;
};

VIRTUAL(AFFFD, FileLikeObject)
     VMETHOD(Con) = AFFFD_Con;
END_VIRTUAL

AFFFD Image_Con(AFFFD self, char *stream_name) {
  printf("Constructing an image %s\n", stream_name);
  return self;
};

VIRTUAL(Image, AFFFD)
     VMETHOD(super.Con) = Image_Con;
END_VIRTUAL
