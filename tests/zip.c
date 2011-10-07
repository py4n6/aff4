/*************************************************
This file tests the zip file volume implementation.
***************************************************/

#include "aff4_internal.h"

extern char TEMP_DIR[];

TEST(ZipTest) {
  Resolver resolver = AFF4_get_resolver(NULL, NULL);
  ZipFile zip;
  RDFURN urn = new_RDFURN(resolver);
  FileLikeObject segment;

  // Make a Zip volume
  zip = (ZipFile)CALL(resolver, create, NULL, AFF4_ZIP_VOLUME);
  CALL(zip->storage_urn, set, TEMP_DIR);
  CALL(zip->storage_urn, add, "ZipTest.zip");
  CALL((AFFObject)zip, finish);

  // Open a new member
  CALL(urn, set, "foobar");
  segment = CALL((AFF4Volume)zip, open_member, urn, 'w', 0);

  CALL(resolver, manage, (AFFObject)zip);

  CALL(segment, write, ZSTRING("hello"));

  CALL((AFFObject)segment, close);

  CALL((AFFObject)zip, close);
};
