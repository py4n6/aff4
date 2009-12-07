#include "aff4.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

#define TEST_FILE "/tmp/test.zip"

//#define RESOLVER_TESTS 1
//#define ZIPFILE_TESTS 1
//#define MAP_TESTS 1
#define ENCRYPTED_TESTS 1

#ifdef RESOLVER_TESTS
#define URN "aff4://hello"
#define ATTRIBUTE "aff4://cruel"
#define VALUE "world"

void resolver_test_1() {
  RDFURN urn = new_RDFURN(NULL);
  XSDInteger xsd_integer = new_XSDInteger(urn);

  // Clearing the urn
  CALL(urn, set, URN);
  CALL(oracle, del, urn, ATTRIBUTE);

  printf("Setting integer\n");
  CALL(xsd_integer, set, 5);
  CALL(oracle, add_value, urn, ATTRIBUTE, (RDFValue)xsd_integer);
  CALL(xsd_integer, set, 15);
  CALL(oracle, add_value, urn, ATTRIBUTE, (RDFValue)xsd_integer);

  CALL(xsd_integer, set, 0);

  // Retrieve it
  {
    RESOLVER_ITER *iter;

    iter = CALL(oracle, get_iter, urn, urn, ATTRIBUTE);
    while(CALL(oracle, iter_next, iter, (RDFValue)xsd_integer)) {
      printf("Retrieved value: %llu\n", xsd_integer->value);
    };
  };

  // Retrieve it using allocation
  {
    RESOLVER_ITER *iter;
    RDFValue result;

    iter = CALL(oracle, get_iter, urn, urn, ATTRIBUTE);
    while(1) {
      result = CALL(oracle, iter_next_alloc, &iter);
      if(!result) break;

      printf("Got value of type '%s', value '%s'\n", result->dataType,
	     CALL(result, serialise));

      aff4_free(result);
    };
  };

  return;
};

// Test locking:
void resolver_test_locks() {
  RDFURN urn = new_RDFURN(NULL);

  int pid = fork();
  // Parent
  if(pid) {
    uint32_t now = time(NULL);

    printf("Parent %lu Getting lock on %s\n", now, URN);
    CALL(oracle, lock, urn, 'w');
    printf("Parent %lu Got lock on %s\n", time(NULL) - now, URN);
    sleep(5);
    printf("Parent %lu Releasing lock on %s\n", time(NULL) - now, URN);
    CALL(oracle, unlock, URN, 'w');
  } else {
    uint32_t now = time(NULL);

    // Children have to get their own Resolver
    aff4_free(oracle);
    oracle = CONSTRUCT(Resolver, Resolver, Con, NULL);

    // Give the parent a chance
    sleep(1);
    printf("Child %lu Getting lock on %s\n", now, URN);
    CALL(oracle, lock, URN, 'w');
    printf("Child %lu Got lock on %s\n", time(NULL) - now, URN);
    sleep(5);
    printf("Child %lu Releasing lock on %s\n", time(NULL) - now, URN);
    CALL(oracle, unlock, URN, 'w');
    // Done
    exit(0);
  };
};

#endif

#define FILENAME "/tmp/test.zip"

#ifdef ZIPFILE_TESTS
/** First test builds a new zip file from /bin/ls */
void zipfile_test1() {
  // Make a new zip file
  ZipFile zip = open_volume(FILENAME, 'w');
  FileLikeObject out_fd;

  CALL(zip, writestr, "hello", ZSTRING("hello world"), 0);

  // Open the member foobar for writing
  out_fd = CALL(zip, open_member, "foobar", 'w',
		ZIP_DEFLATE);

  // It worked - now copy /bin/ls into it
  if(out_fd) {
    char buffer[BUFF_SIZE];
    int fd=open("/bin/ls",O_RDONLY);
    int length;
    while(1) {
      length = read(fd, buffer, BUFF_SIZE);
      if(length == 0) break;

      CALL(out_fd, write, buffer, length);
    };

    // Close the member (finalises the member)
    CALL(out_fd, close);
  };

  // Close the archive
  CALL(zip, close);
  aff4_free(zip);
};

void zipfile_test2() {
  ZipFile zip = (ZipFile)CALL(oracle, create, AFF4_ZIP_VOLUME);
  // Now make an image object
  FileLikeObject outfd = (FileLikeObject)CALL(oracle, create, AFF4_IMAGE);
  RDFURN file_urn = new_RDFURN(NULL);

  CALL(file_urn, set, FILENAME);

  CALL(oracle, set_value, URNOF(zip), AFF4_STORED, (RDFValue)file_urn);

  zip = (ZipFile)CALL((AFFObject)zip, finish);

  CALL(URNOF(outfd), set, STRING_URNOF(zip));
  CALL(URNOF(outfd), add, "foobar_image");

  CALL(oracle, set_value, URNOF(outfd), AFF4_STORED,
       (RDFValue)URNOF(zip));

  CALL(oracle, set_value, URNOF(outfd), AFF4_CHUNK_SIZE,
       rdfvalue_from_int(file_urn,1024));

  CALL(oracle, set_value, URNOF(outfd), AFF4_CHUNKS_IN_SEGMENT,
       rdfvalue_from_int(file_urn, 10));

  outfd = (FileLikeObject)CALL((AFFObject)outfd, finish);

  if(outfd) {
    char buffer[BUFF_SIZE];
    int fd=open("/bin/ls",O_RDONLY);
    int length;
    while(1) {
      length = read(fd, buffer, BUFF_SIZE);
      if(length == 0) break;

      CALL(outfd, write, buffer, length);
    };

    // Close the member (finalises the member)
    CALL(outfd, close);
  };

  // Close the archive
  CALL(zip, close);

  aff4_free(file_urn);
};

void zipfile_test_load() {
  ZipFile zip = (ZipFile)CALL(oracle, create, AFF4_ZIP_VOLUME);
  RDFURN location = new_RDFURN(zip);

  CALL(location, set, FILENAME);
  CALL(zip, load_from, location, 'r');

};

#define TIMES 1000

/** Try to create a new ZipFile.

When creating a new AFFObject we:

1) Ask the oracle to create it (providing the class pointer).

2) Set all the required and optional parameters.

3) Call the finish method. If it succeeds we have a fully operational
object. If it fails (returns NULL), we may have failed to set some
parameters.

*/
#if 0
void test1_5() {
  ZipFile zipfile;

  // Now create a new AFF2 file on top of it
  zipfile = (ZipFile)CALL(oracle, create, AFF4_ZIP_VOLUME);
  CALL((AFFObject)zipfile, set_property, "aff2:stored", "file://" TEST_FILE);

  if(CALL((AFFObject)zipfile, finish)) {
    char buffer[BUFF_SIZE];
    int fd=open("/bin/ls",O_RDONLY);
    int length;
    FileLikeObject out_fd = CALL((ZipFile)zipfile,
				 open_member, "foobar", 'w',
				 ZIP_DEFLATE);

    if(!out_fd) return;

    while(1) {
      length = read(fd, buffer, BUFF_SIZE);
      if(length == 0) break;

      CALL(out_fd, write, buffer, length);
    };
    CALL(out_fd,close);

    CALL((ZipFile)zipfile, writestr, "hello",
	 ZSTRING("hello world"), 0);

    CALL((ZipFile)zipfile,close);
  };

  CALL(oracle, cache_return, (AFFObject)zipfile);
};
#endif

/** This tests the cache for reading zip members.

There are two steps:

1) We open the zip file directly to populate the oracle.

2) We ask the oracle to open anything it knows about.

If you have a persistent oracle you dont need to use step 1 at all
since the information is already present.

*/
void test2() {
  int i;
  struct timeval epoch_time;
  struct timeval new_time;
  int diff;
  FileLikeObject fd;
  ZipFile zipfile = CONSTRUCT(ZipFile, ZipFile, Con, NULL, "file://" TEST_FILE, 'r');

  // This is only needed to populate the oracle
  if(!zipfile) return;
  CALL(oracle, cache_return, (AFFObject)zipfile);

  // Now ask the resolver for the different files
  gettimeofday(&epoch_time, NULL);

  for(i=0;i<TIMES;i++) {
    fd = (FileLikeObject)CALL(oracle, open,  "hello", 'r');
    if(!fd) {
      RaiseError(ERuntimeError, "Error reading member");
      return;
    };

    CALL(oracle, cache_return, (AFFObject)fd);
  };

  CALL(fd, get_data);
  gettimeofday(&new_time, NULL);
  printf("Resolving foobar produced **************\n%s\n******************\n", fd->data);
  diff = (new_time.tv_sec * 1000 + new_time.tv_usec/1000) -
    (epoch_time.tv_sec * 1000 + epoch_time.tv_usec/1000);
  printf("Decompressed foobar %d times in %d mseconds (%f)\n",
	 TIMES,  diff,
	 ((float)diff)/TIMES);
};

#endif

#ifdef IMAGE_TESTS
/** This test writes a two part AFF2 file.

First we ask the oracle to create a FileBackedObject then attach that
to a ZipFile volume. We then create an Image stream and attach that to
the ZipFile volume.

*/
void test_image_create() {
  ZipFile zipfile = (ZipFile)CALL(oracle, create, AFF4_ZIP_VOLUME);
  Image image;
  char buffer[BUFF_SIZE];
  int in_fd;
  int length;
  Link link;
  char zipfile_urn[BUFF_SIZE];

  if(!zipfile) goto error;

  // Make local copy of zipfile's URN
  strncpy(zipfile_urn, URNOF(zipfile), BUFF_SIZE);

  CALL((AFFObject)zipfile, set_property, "aff2:stored", "file://" TEST_FILE);

  // Is it ok?
  if(!CALL((AFFObject)zipfile, finish))
    goto error;

  // Finished with it for the moment
  //  CALL(oracle, cache_return, (AFFObject)zipfile);

  // Now we need to create an Image stream
  image = (Image)CALL(oracle, create, AFF4_IMAGE);

  // Tell the image that it should be stored in the volume
  CALL((AFFObject)image, set_property, "aff2:stored", zipfile_urn);
  CALL((AFFObject)image, set_property, "aff2:chunks_in_segment", "2");

  // Is it ok?
  if(!CALL((AFFObject)image, finish))
    goto error;

  in_fd=open("/bin/ls",O_RDONLY);
  while(in_fd >= 0) {
    length = read(in_fd, buffer, BUFF_SIZE);
    if(length == 0) break;

    CALL((FileLikeObject)image, write, buffer, length);
  };

  CALL((FileLikeObject)image, close);

  // Close the zipfile - get it back
  //  zipfile = (ZipFile)CALL(oracle, open, zipfile_urn);
  CALL((ZipFile)zipfile, close);

 error:
  // We are done with that now
  CALL(oracle, cache_return, (AFFObject)image);
  CALL(oracle, cache_return, (AFFObject)zipfile);

};

/** Test reading of the Image stream.

We need to open the aff file in order to populate the oracle.
*/
void test_image_read() {
  char *link_name = "default";
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, Con, NULL, TEST_FILE, 'r');
  Image image;
  int outfd;
  char buff[BUFF_SIZE];
  int length;
  ZipFile zipfile;

  if(!fd) {
    RaiseError(ERuntimeError, "Unable to open file %s", TEST_FILE);
    return;
  };

  zipfile =CONSTRUCT(ZipFile, ZipFile, Con, fd, URNOF(fd), 'r');
  if(!zipfile) {
    RaiseError(ERuntimeError, "%s is not a zip file?", TEST_FILE);
    aff4_free(fd);
    return;
  };

  // We just put it in the cache anyway
  CALL(oracle, cache_return, (AFFObject)zipfile);

  image = (Image)CALL(oracle, open, link_name, 'r');
  if(!image) {
    RaiseError(ERuntimeError, "Unable to find stream %s", link_name);
    goto error;
  };

  outfd = creat("output.dd", 0644);
  if(outfd<0) goto error;

  while(1) {
    length = CALL((FileLikeObject)image, read, buff, BUFF_SIZE);
    if(length<=0) break;

    write(outfd, buff, length);
  };

  close(outfd);

 error:
  CALL(oracle, cache_return, (AFFObject)image);
  return;
};

/** A little helper that copies a file into a volume */
char *create_image(char *volume, char *filename, char *friendly_name) {
  Image image;
  int in_fd;
  char buffer[BUFF_SIZE * 10];
  int length;
  Link link;

  // Now we need to create an Image stream
  image = (Image)CALL(oracle, create, AFF4_IMAGE);
  if(!image) return NULL;

  // Tell the image that it should be stored in the volume
  CALL((AFFObject)image, set_property, "aff2:stored", volume);
  CALL((AFFObject)image, set_property, "aff2:chunks_in_segment", "256");

  // Is it ok?
  if(!CALL((AFFObject)image, finish))
    return NULL;

  in_fd=open(filename, O_RDONLY);
  while(in_fd >= 0) {
    length = read(in_fd, buffer, BUFF_SIZE);
    if(length == 0) break;

    CALL((FileLikeObject)image, write, buffer, length);
  };

  CALL((FileLikeObject)image, close);

  return URNOF(image);
};

#endif

#define CHUNK_SIZE 32*1024


#ifdef MAP_TESTS

/** This tests the Map Image - we create an AFF file containing 3
    seperate streams and build a map. Then we read the map off and
    copy it into the output.
*/
#define D1  "d1.dd"
#define D2  "d2.dd"
#define D3  "d3.dd"
#define MAP_NAME "test_map"

void test_map_create() {
  MapDriver map;
  ZipFile zipfile = (ZipFile)CALL(oracle, create, AFF4_ZIP_VOLUME, 'w');
  CALL(oracle, set_value, URNOF(zipfile), AFF4_STORED, rdfvalue_from_urn(zipfile, FILENAME));

  zipfile = (ZipFile)CALL((AFFObject)zipfile, finish);
  if(!zipfile) return;

  CALL(oracle, cache_return, (AFFObject)zipfile);

  // Now create a map stream:
  map = (MapDriver)CALL(oracle, create, AFF4_MAP, 'w');
  if(map) {
    URNOF(map) = CALL(URNOF(zipfile), copy, map);
    CALL(URNOF(map), add, MAP_NAME);

    CALL((AFFObject)map, set_property, AFF4_STORED,
	 (RDFValue)URNOF(zipfile));

    CALL((AFFObject)map, set_property, AFF4_TARGET_PERIOD,
	 (RDFValue)rdfvalue_from_int(zipfile, 3));

    CALL((AFFObject)map, set_property, AFF4_IMAGE_PERIOD,
	 (RDFValue)rdfvalue_from_int(zipfile, 6));

    CALL((AFFObject)map, set_property, AFF4_SIZE,
	 (RDFValue)rdfvalue_from_int(zipfile, 5242880 * 2));

    CALL((AFFObject)map, set_property, AFF4_BLOCKSIZE,
	 (RDFValue)rdfvalue_from_int(zipfile, 64 * 1024));

    map = (MapDriver)CALL((AFFObject)map, finish);
  };

  if(!map) {
    RaiseError(ERuntimeError, "Unable to create a map stream?");
    goto exit;
  };

  // Create the raid reassembly map
  CALL(map, add, 0, 0, D2);
  CALL(map, add, 1, 0, D1);
  CALL(map, add, 2, 1, D3);
  CALL(map, add, 3, 1, D2);
  CALL(map, add, 4, 2, D1);
  CALL(map, add, 5, 2, D3);

  CALL((FileLikeObject)map, close);

 exit:
  zipfile = (ZipFile)CALL(oracle, open, URNOF(zipfile), 'w');
  CALL(zipfile, close);
};

#define TEST_BUFF_SIZE 63*1024
void test_map_read() {
  MapDriver map;
  int outfd, length;
  char buff[TEST_BUFF_SIZE];
  RDFURN volume = new_RDFURN(NULL);
  RDFURN filename = (RDFURN)rdfvalue_from_urn(volume, FILENAME);
  RDFURN map_urn;

  if(!CALL(oracle, resolve_value, filename, AFF4_VOLATILE_CONTAINS, (RDFValue)volume))
    goto error;

  map_urn = CALL(volume, copy, volume);
  CALL(map_urn, add, MAP_NAME);

  map = (MapDriver)CALL(oracle, open, map_urn, 'r');
  if(!map) goto error;

  outfd = creat("test_output.dd", 0644);
  if(outfd<0) goto error;

  while(1) {
    length = CALL((FileLikeObject)map, read, buff, TEST_BUFF_SIZE);
    if(length<=0) break;

    write(outfd, buff, length);
  };

  close(outfd);

  CALL(oracle, cache_return, (AFFObject)map);

 error:
  aff4_free(volume);
  return;
};
#endif

#if HTTP_TESTS==1 && HAVE_LIBCURL==1
void test_http_handle() {
  FileLikeObject http = (FileLikeObject)CONSTRUCT(HTTPObject, HTTPObject,
						  Con, NULL, "http://127.0.0.1/test.c");
  char buff[BUFF_SIZE];

  CALL(http, read, buff, 100);

};
#endif

#ifdef ENCRYPTED_TESTS
#define PASSPHRASE "Hello"

void test_cipher() {
  AES256Password cipher;
  ZipFile zip = (ZipFile)CALL(oracle, create, AFF4_ZIP_VOLUME, 'w');
  // Now make an image object
  FileLikeObject image = (FileLikeObject)CALL(oracle, create, AFF4_IMAGE, 'w');
  RDFURN file_urn = new_RDFURN(NULL);
  FileLikeObject encrypted;

  CALL(file_urn, set, FILENAME);

  CALL(oracle, set_value, URNOF(zip), AFF4_STORED, (RDFValue)file_urn);

  zip = (ZipFile)CALL((AFFObject)zip, finish);

  // Done with the volume
  CALL(oracle, cache_return, (AFFObject)zip);

  { // Make the image
    CALL(URNOF(image), set, STRING_URNOF(zip));
    CALL(URNOF(image), add, "foobar_image/storage");

    CALL(oracle, set_value, URNOF(image), AFF4_STORED,
         (RDFValue)URNOF(zip));

    image = (FileLikeObject)CALL((AFFObject)image, finish);
    CALL(oracle, cache_return, (AFFObject)image);
  }

  if(image){ // Make the encrypted stream
    cipher = new_rdfvalue(NULL, AFF4_AES256_PASSWORD);
    // Set the password
    CALL(cipher, set, PASSPHRASE);

    encrypted = (FileLikeObject)CALL(oracle, create, AFF4_ENCRYTED, 'w');

    CALL(URNOF(encrypted), set, STRING_URNOF(zip));
    CALL(URNOF(encrypted), add, "foobar_image");

    CALL((AFFObject)encrypted, set_property, AFF4_STORED, (RDFValue)URNOF(zip));
    CALL((AFFObject)encrypted, set_property, AFF4_TARGET, (RDFValue)URNOF(image));
    CALL((AFFObject)encrypted, set_property, AFF4_CIPHER, (RDFValue)cipher);
    encrypted = CALL((AFFObject)encrypted, finish);
  };

  if(encrypted) {
    char buffer[BUFF_SIZE];
    int fd=open("/bin/ls",O_RDONLY);
    int length;
    while(1) {
      length = read(fd, buffer, BUFF_SIZE);
      if(length == 0) break;

      CALL(encrypted, write, buffer, length);
    };

    // Close the member (finalises the member)
    CALL(encrypted, close);
  };

  // Close the backing stream
  CALL(image, close);

  // Close the archive
  CALL(zip, close);

  aff4_free(file_urn);

};

void test_cipher_read() {
  ZipFile zip = open_volume(FILENAME);
  FileLikeObject encrypted;
  RDFURN encrypted_urn = new_RDFURN(zip);
  char buff[BUFF_SIZE];
  int len;
  int fd;

  setenv(AFF4_VOLATILE_PASSPHRASE, PASSPHRASE,1);

  if(zip) {
    CALL(encrypted_urn, set, STRING_URNOF(zip));
    CALL(encrypted_urn, add, "foobar_image");

    encrypted = CALL(oracle, open, encrypted_urn, 'r');
    if(!encrypted) return;

    fd = open("/tmp/ls.test", O_CREAT | O_RDWR | O_BINARY, S_IRWXU | S_IRWXG | S_IRWXO);

    while(fd > 0) {
      len = CALL(encrypted, read, buff, BUFF_SIZE);
      if(len == 0) break;

      write(fd, buff, len);
    };

    close(fd);

    CALL(oracle,cache_return, (AFFObject)encrypted);
  };
};

#endif

int main() {
  talloc_enable_leak_report_full();
  AFF4_TDB_FLAGS |= TDB_CLEAR_IF_FIRST;

  AFF4_Init();

  AFF4_DEBUG_LEVEL = 10;

#ifdef RESOLVER_TESTS
  {
    XSDDatetime t = new_XSDDateTime(NULL);
    struct timeval now;

    gettimeofday(&now, NULL);
    CALL(t,set, now);

    printf("Time now is %s\n", (char *)CALL((RDFValue)t, serialise));

    aff4_free(t);
  };

  //resolver_test_1();
  resolver_test_locks();
#endif

#ifdef ZIPFILE_TESTS
  //zipfile_test1();
  zipfile_test2();
  zipfile_test_load();
#endif

#ifdef MAP_TESTS
  test_map_create();
  test_map_read();
#endif

#ifdef ENCRYPTED_TESTS
  //test_cipher();
  test_cipher_read();
#endif

  return 0;
};
