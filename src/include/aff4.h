/** This is the public include file for using libaff4 */

/** 
    Opens the URIs specified in images and uses them to populate the
    resolver. We then try to open the stream.

    images may be omitted in which case we will use exiting
    information in the universal resolver.

    If stream is not specified (NULL), we attempt to open the stream
    "default" in each image.

    We return an opaque handle to the stream or NULL if failed.
*/
typedef void * AFF4_HANDLE;

AFF4_HANDLE aff4_open(char **images);
void aff4_seek(AFF4_HANDLE self, uint64_t offset, int whence);
uint64_t aff4_tell(AFF4_HANDLE self);
int aff4_read(AFF4_HANDLE self, char *buf, int len);
void aff4_close(AFF4_HANDLE self);
