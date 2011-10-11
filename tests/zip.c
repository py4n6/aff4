/*************************************************
This file tests the zip file volume implementation.
***************************************************/

#include "aff4_internal.h"

extern char TEMP_DIR[];

TEST(ZipTestWriter) {
  Resolver resolver = AFF4_get_resolver(NULL, NULL);
  ZipFile zip;
  RDFURN urn;
  FileLikeObject segment;

  // Make a Zip volume
  zip = (ZipFile)CALL(resolver, create, NULL, AFF4_ZIP_VOLUME, 'w');
  CALL(zip->storage_urn, set, TEMP_DIR);
  CALL(zip->storage_urn, add, "ZipTest.zip");
  CALL((AFFObject)zip, finish);

  // Open a new member
  urn = CALL((RDFValue)URNOF(zip), clone, resolver);
  CALL(urn, add, "foobar");

  segment = CALL((AFF4Volume)zip, open_member, urn, 'w', ZIP_DEFLATE);

  CALL(segment, write, ZSTRING("hello"));

  CALL((AFFObject)segment, close);

  CALL((AFFObject)zip, close);

  talloc_free(zip);
  talloc_free(resolver);
};

TEST(ZipTestReader) {
  Resolver resolver = AFF4_get_resolver(NULL, NULL);
  ZipFile zip;
  RDFURN urn;
  FileLikeObject segment, fd;
  char buffer[BUFF_SIZE];
  int length;

  // Make a Zip volume
  zip = (ZipFile)CALL(resolver, create, NULL, AFF4_ZIP_VOLUME, 'r');
  CALL(zip->storage_urn, set, "file:///tmp/ZipTest.zip");

  // This will return 0 if we fail to open
  CU_ASSERT_FATAL(CALL((AFFObject)zip, finish) > 0);

  urn = CALL((RDFValue)URNOF(zip), clone, resolver);
  CALL(urn, add, "foobar");
  segment = CALL((AFF4Volume)zip, open_member, urn, 'r', 0);

  length = CALL(segment, read, buffer, BUFF_SIZE);
  printf("Read %d\n", length);

  CU_ASSERT_FATAL(length == 6);
  CU_ASSERT(!memcmp(buffer, ZSTRING_NO_NULL("hello")));

  CALL((AFFObject)zip, close);
  talloc_free(zip);
  talloc_free(resolver);

};
