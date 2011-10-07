/*************************************************
This file tests the various aff4 objects.
***************************************************/

#include "aff4_internal.h"

extern char TEMP_DIR[];


INIT() {
  return 0;
};

/*************************************************
Test the FileBackedObject
***************************************************/
TEST(FileBackedObjectTest) {
  Resolver oracle = AFF4_get_resolver(NULL, NULL);
  FileLikeObject fd;
  RDFURN urn = new_RDFURN(oracle);
  XSDInteger size;
  char buff[BUFF_SIZE];

  memset(buff, 0, sizeof(buff));

  CALL(urn, set, TEMP_DIR);
  CALL(urn, add, "FileBackedObject.dd");

  // Cant open non-existent files.
  fd = (FileLikeObject)CALL(oracle, open, urn, 'r');
  CU_ASSERT_PTR_NULL(fd);

  // Create the file.
  fd = (FileLikeObject)CALL(oracle, create, urn, AFF4_FILE);

  /* That should succeed */
  CU_ASSERT_PTR_NOT_NULL(fd);

  /* Configure the object */
  CALL((AFFObject)fd, finish);
  CU_ASSERT_EQUAL(fd->readptr, 0);

  URNOF(fd) = urn;
  CALL((AFFObject)fd, finish);

  /* Write something */
  CALL(fd, write, ZSTRING("hello"));

  CU_ASSERT_EQUAL(fd->readptr, 6);

  // Need to update the size
  size = (XSDInteger)CALL((AFFObject)fd, resolve, urn, AFF4_SIZE);
  CU_ASSERT_EQUAL(size->value, 6);

  // Seek
  CALL(fd, seek, 1, 0);
  CALL(fd, write, "i", 1);
  CU_ASSERT_EQUAL(fd->readptr, 2);

  // Read
  CALL(fd, seek, 0, 0);
  CALL(fd, read, buff, 2);
  CU_ASSERT_STRING_EQUAL(buff, "hi");

  // Seek from the end
  CALL(fd, seek, -3, 2);
  CU_ASSERT_EQUAL(fd->readptr, 3);

  // truncate
  CALL(fd, truncate, 2);

  size = (XSDInteger)CALL((AFFObject)fd, resolve, urn, AFF4_SIZE);
  CU_ASSERT_EQUAL(size->value, 2);
  CU_ASSERT_EQUAL(fd->readptr, 2);

  talloc_free(oracle);
};
