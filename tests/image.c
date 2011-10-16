/*************************************************
This file tests the zip file volume implementation.
***************************************************/

#include "aff4_internal.h"

extern char TEMP_DIR[];


TEST(ImageWriter) {
  Resolver resolver = AFF4_get_resolver(NULL, NULL);
  FileLikeObject image = (FileLikeObject)CALL(resolver, create, NULL, AFF4_IMAGE, 'w');
  ZipFile zip = (ZipFile)CALL(resolver, create, NULL, AFF4_ZIP_VOLUME, 'w');
  int i;

  CALL(zip->storage_urn, set, TEMP_DIR);
  CALL(zip->storage_urn, add, "Image.zip");
  CALL((AFFObject)zip, finish);
  CALL(resolver, cache_return, (AFFObject)zip);

  ((AFF4Image)image)->stored = URNOF(zip);

  /* An extremely small chunksize for testing. This forces a new bevy
     every 320 bytes.
  */
  ((AFF4Image)image)->chunk_size = 32;
  ((AFF4Image)image)->chunks_in_segment = 10;
  ((AFF4Image)image)->thread_count = 10;

  CALL((AFFObject)image, finish);

  /* Write some data. */
  for(i=0; i<1000; i++) {
    CALL(image, write, ZSTRING("hello world!"));
  };

  CALL((AFFObject)image, close);
  talloc_free(image);

  CALL((AFFObject)zip, close);
  talloc_free(zip);

 exit:
  talloc_free(resolver);
};
