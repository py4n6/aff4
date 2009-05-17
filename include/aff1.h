#include <afflib/afflib.h>

// Optional AFF1 legacy volume support
CLASS(AFF1Volume, ZipFile)
     AFFILE *handle;
END_CLASS

CLASS(AFF1Stream, FileLikeObject)
     char *volume_urn;
END_CLASS
