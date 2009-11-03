#include "zip.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <assert.h>

#define TEST_FILE "test.zip"

//#define RESOLVER_TESTS 1
#define ZIPFILE_TESTS 1

#ifdef RESOLVER_TESTS
#define URN "aff4://hello"
#define ATTRIBUTE "aff4://cruel"
#define VALUE "world"

void resolver_test_1() {
  char *ctx = talloc_size(NULL, 1);
  char **result;
  char **i;
  char *value;
  uint32_t integer, *integer_p = 0x18;

  // Clearing the urn
  CALL(oracle, del, URN, ATTRIBUTE);

  // Set an attribute
  printf("Setting string \n");
  CALL(oracle, set, URN, ATTRIBUTE, VALUE, RESOLVER_DATA_STRING);
  
  // Retrieve it
  value = (char *)CALL(oracle, resolve, ctx, URN, ATTRIBUTE,
		       RESOLVER_DATA_STRING);
  printf("Retrieved value: %s\n", value);

  printf("Setting uint32_t\n");
  integer = 0x18;
  CALL(oracle, set, URN, "integer", &integer, RESOLVER_DATA_UINT32);

  integer_p = (uint32_t *) CALL(oracle, resolve, ctx, URN, "integer",
				RESOLVER_DATA_UINT32);
  printf("Retrieved value: %X\n", *integer_p);

  // Retrieved back what we put in
  assert(!strcmp(VALUE, value));

  // Add another value
  CALL(oracle, add, URN, ATTRIBUTE, "world2", RESOLVER_DATA_STRING, 1);

  // Grab the list of values
  {
    RESOLVER_ITER iter;
    char result[BUFF_SIZE];

    CALL(oracle, get_iter, &iter, URN, ATTRIBUTE, RESOLVER_DATA_STRING);
    while(CALL(oracle, iter_next, &iter, result, BUFF_SIZE)) {
      printf("Retrieved values: %s\n", result);
    };
  };

  talloc_free(ctx);
};

// Test locking:
void resolver_test_locks() {
  int pid = fork();
  // Parent
  if(pid) {
    uint32_t now = time(NULL);

    printf("Parent %lu Getting lock on %s\n", now, URN);
    CALL(oracle, lock, URN, 'w');
    printf("Parent %lu Got lock on %s\n", time(NULL) - now, URN);
    sleep(5);
    printf("Parent %lu Releasing lock on %s\n", time(NULL) - now, URN);
    CALL(oracle, unlock, URN, 'w');
  } else {
    uint32_t now = time(NULL);

    // Children have to get their own Resolver
    talloc_free(oracle);
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

#ifdef ZIPFILE_TESTS
#define FILENAME "test.zip"
/** First test builds a new zip file from /bin/ls */
void zipfile_test1() {
  // Make a new zip file
  ZipFile zip = open_volume(FILENAME, 'w');
  FileLikeObject out_fd;

  CALL(zip, writestr, "hello", ZSTRING("hello world"), NULL, 0, 0);

  // Open the member foobar for writing
  out_fd = CALL(zip, open_member, "foobar", 'w', NULL, 0,
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
  talloc_free(zip);
};

void zipfile_test2() {
  ZipFile zip = (ZipFile)CALL(oracle, create, (AFFObject *)GETCLASS(ZipFile), 'w');
  // Now make an image object
  FileLikeObject outfd = (FileLikeObject)CALL(oracle, create, (AFFObject *)GETCLASS(Image), 'w');
  
  CALL(oracle, set, URNOF(zip), AFF4_STORED, "file://" FILENAME, RESOLVER_DATA_URN);
  zip = (ZipFile)CALL((AFFObject)zip, finish);

  URNOF(outfd) = talloc_asprintf(outfd, "%s/foobar_image", URNOF(zip));

  CALL(oracle, set, URNOF(outfd), AFF4_STORED, URNOF(zip), RESOLVER_DATA_URN);
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
void test1_5() {
  ZipFile zipfile;

  // Now create a new AFF2 file on top of it
  zipfile = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile, 'w');
  CALL((AFFObject)zipfile, set_property, "aff2:stored", "file://" TEST_FILE);

  if(CALL((AFFObject)zipfile, finish)) {
    char buffer[BUFF_SIZE];
    int fd=open("/bin/ls",O_RDONLY);
    int length;
    FileLikeObject out_fd = CALL((ZipFile)zipfile, 
				 open_member, "foobar", 'w', NULL, 0, 
				 ZIP_DEFLATE);

    if(!out_fd) return;

    while(1) {
      length = read(fd, buffer, BUFF_SIZE);
      if(length == 0) break;
      
      CALL(out_fd, write, buffer, length);
    };
    CALL(out_fd,close);

    CALL((ZipFile)zipfile, writestr, "hello",
	 ZSTRING("hello world"), NULL, 0, 0);

    CALL((ZipFile)zipfile,close);
  };

  CALL(oracle, cache_return, (AFFObject)zipfile);
};

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
  ZipFile zipfile = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
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
  image = (Image)CALL(oracle, create, (AFFObject *)&__Image);
  
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

  // We want to make it easy to locate this image so we set up a link
  // to it:
  link = (Link)CALL(oracle, create, (AFFObject *)&__Link);
  // The link will be stored in this zipfile
  CALL((AFFObject)link, set_property, "aff2:stored", zipfile_urn);
  CALL(link, link, oracle, zipfile_urn, URNOF(image), "default");
  CALL((AFFObject)link, finish);
  CALL(oracle, cache_return, (AFFObject)link);

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
    talloc_free(fd);
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
  image = (Image)CALL(oracle, create, (AFFObject *)&__Image);
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

  // We want to make it easy to locate this image so we set up a link
  // to it:
  link = (Link)CALL(oracle, create, (AFFObject *)&__Link);
  // The link will be stored in this zipfile
  CALL((AFFObject)link, set_property, "aff2:stored", volume);
  CALL(link, link, oracle, volume, URNOF(image), friendly_name);
  CALL((AFFObject)link, finish);
  CALL(oracle, cache_return, (AFFObject)link);
  
  return URNOF(image);
};

#endif

#define CHUNK_SIZE 32*1024


#ifdef MAP_TESTS

/** This tests the Map Image - we create an AFF file containing 3
    seperate streams and build a map. Then we read the map off and
    copy it into the output.
*/
#define IMAGES "images/"
#define D0  "d1.dd"
#define D1  "d2.dd"
#define D2  "d3.dd"

void test_map_create() {
  MapDriver map;
  FileLikeObject fd;
  char *d0;
  char *d1;
  char *d2;
  char *volume;
  ZipFile zipfile = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  CALL((AFFObject)zipfile, set_property, "aff2:stored", "file://" TEST_FILE);
  
  if(!CALL((AFFObject)zipfile, finish))
    return;

  volume = URNOF(zipfile);
  CALL(oracle, cache_return, (AFFObject)zipfile);

  d0=create_image(volume, IMAGES D0, D0);
  d1=create_image(volume, IMAGES D1, D1);
  d2=create_image(volume, IMAGES D2, D2);

  // Now create a map stream:
  map = (MapDriver)CALL(oracle, create, (AFFObject *)&__MapDriver);
  if(map) {
    CALL((AFFObject)map, set_property, AFF4_STORED, volume);
    CALL((AFFObject)map, set_property, AFF4_TARGET_PERIOD, "3");
    CALL((AFFObject)map, set_property, AFF4_IMAGE_PERIOD, "6");
    CALL((AFFObject)map, set_property, AFF4_BLOCKSIZE, "64k");
    map = (MapDriver)CALL((AFFObject)map, finish);
  };

  if(!map) {
    RaiseError(ERuntimeError, "Unable to create a map stream?");
    goto exit;
  };

  // Create the raid reassembly map
  CALL(map, add, 0, 0, D1);
  CALL(map, add, 1, 0, D0);
  CALL(map, add, 2, 1, D2);
  CALL(map, add, 3, 1, D1);
  CALL(map, add, 4, 2, D0);
  CALL(map, add, 5, 2, D2);

  fd = (FileLikeObject)CALL(oracle, open, D1, 'r');
  CALL((AFFObject)map, set_property, AFF4_SIZE, from_int(fd->size * 2));
  CALL(oracle, cache_return, (AFFObject)fd);

  CALL(map, save_map);
  CALL((FileLikeObject)map, close);
  CALL(oracle, cache_return, (AFFObject)map);

  // Make a link to the map:
  {
    Link link;
    
    link = (Link)CALL(oracle, create, (AFFObject *)&__Link);
    // The link will be stored in this zipfile
    CALL(link, link, oracle, volume, URNOF(map), "map");
    CALL((AFFObject)link, finish);
    CALL(oracle, cache_return, (AFFObject)link);
  };

 exit:
  zipfile = (ZipFile)CALL(oracle, open, volume, 'w');
  CALL(zipfile, close);
  CALL(oracle, cache_return, (AFFObject)zipfile);
};

#define TEST_BUFF_SIZE 63*1024
void test_map_read(char *filename) {
  MapDriver map;
  int outfd, length;
  char buff[TEST_BUFF_SIZE];
  ZipFile zipfile = CONSTRUCT(ZipFile, ZipFile, Con, NULL,filename, 'r');

  CALL(oracle, cache_return, (AFFObject)zipfile);
  map  = (MapDriver)CALL(oracle, open, "map", 'r');
  if(!map) return;

  outfd = creat("output.dd", 0644);
  if(outfd<0) goto error;

  while(1) {
    length = CALL((FileLikeObject)map, read, buff, TEST_BUFF_SIZE);
    if(length<=0) break;

    write(outfd, buff, length);
  };

  close(outfd);

 error:
  CALL(oracle, cache_return, (AFFObject)map);
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

#ifdef ENCTYPTED_TESTS
void test_encrypted(char *filename) {
  ZipFile container = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  FileLikeObject encrypted_stream, embedded_stream;
  ZipFile embedded_volume;
  char *container_urn = talloc_strdup(NULL, URNOF(container));
  char *encrypted_stream_uri, *embedded_stream_uri, *embedded_volume_uri;

  CALL((AFFObject)container, set_property, "aff2:stored", "file://" TEST_FILE);

  if(!CALL((AFFObject)container, finish))
    return;

  // Create an encrypted stream:
  encrypted_stream = (FileLikeObject)CALL(oracle, create, (AFFObject *)&__Encrypted);
  // The encrypted stream is in the container
  CALL((AFFObject)encrypted_stream, set_property, "aff2:stored", URNOF(container));

  // Create an embedded Image stream inside the encrypted stream:
  embedded_stream = (FileLikeObject)CALL(oracle, create, (AFFObject *)&__Image);
  CALL((AFFObject)embedded_stream, set_property, "aff2:stored", URNOF(container));
  CALL((AFFObject)embedded_stream, set_property, "aff2:compression", "0");
  CALL((AFFObject)encrypted_stream, set_property, "aff2:target", URNOF(embedded_stream));

  if(!CALL((AFFObject)encrypted_stream, finish)) {
    CALL(oracle, cache_return, (AFFObject)container);
    return;
  };

  if(!CALL((AFFObject)embedded_stream, finish)) {
    CALL(oracle, cache_return, (AFFObject)encrypted_stream);
    CALL(oracle, cache_return, (AFFObject)container);
    return;
  };

  // Finished with those
  CALL(oracle, cache_return, (AFFObject)container);
  CALL(oracle, cache_return, (AFFObject)embedded_stream);
  CALL(oracle, cache_return, (AFFObject)encrypted_stream);

  // Create a new volume inside the embedded stream
  embedded_volume = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  CALL((AFFObject)embedded_volume, set_property, AFF4_STORED, 
       URNOF(encrypted_stream));

  if(!CALL((AFFObject)embedded_volume, finish)) {
    CALL(oracle, cache_return, (AFFObject)embedded_stream);
    CALL(oracle, cache_return, (AFFObject)encrypted_stream);
    CALL(oracle, cache_return, (AFFObject)container);
    return;
  };

  encrypted_stream_uri = talloc_strdup(container_urn, URNOF(encrypted_stream));
  embedded_volume_uri = talloc_strdup(container_urn, URNOF(embedded_volume));
  embedded_stream_uri = talloc_strdup(container_urn, URNOF(embedded_stream));

  CALL(oracle, cache_return, (AFFObject)embedded_volume);

  // Now put the image on it:
  create_image(embedded_volume_uri, "/bin/ls", "encrypted");
  
  embedded_volume = (ZipFile)CALL(oracle, open, embedded_volume_uri, 'w');
  CALL(embedded_volume, close);
  CALL(oracle, cache_return, (AFFObject)embedded_volume);

  encrypted_stream = (FileLikeObject)CALL(oracle, open, encrypted_stream_uri, 'w');
  CALL(encrypted_stream, close);
  CALL(oracle, cache_return, (AFFObject)encrypted_stream);

  /*
  embedded_stream = CALL(oracle, open, embedded_stream_uri);
  CALL(embedded_stream, close);
  CALL(oracle, cache_return, (AFFObject)embedded_stream);
  */

  container = (ZipFile)CALL(oracle, open, container_urn, 'w');
  CALL(container, close);
  CALL(oracle, cache_return, (AFFObject)container);
};
#endif

int main() {
  talloc_enable_leak_report_full();
  AFF4_Init();

#ifdef RESOLVER_TESTS
  resolver_test_1();
  //resolver_test_locks();
#endif

#ifdef ZIPFILE_TESTS
  //  zipfile_test1();
  zipfile_test2();
#endif

  /*
  AFF2_Init();
  ClearError();
  test1_5();
  PrintError();

  AFF2_Init();
  ClearError();
  test2();
  PrintError();

  AFF2_Init();
  ClearError();
  test_image_create();
  PrintError();

  AFF2_Init();
  ClearError();
  test_image_read();
  PrintError();
 
  AFF2_Init();
  ClearError();
  printf("\n*******************\ntest 5\n********************\n");
  test_map_create();
  PrintError();

  AFF2_Init();
  ClearError();
  test_map_read( "file://" TEST_FILE);
  PrintError();

  AFF2_Init();
  ClearError();
  test_map_read("http://127.0.0.1/" TEST_FILE);
  PrintError();

  AFF2_Init();
  ClearError();
  test_encrypted(TEST_FILE);
  PrintError();
  */  
  return 0;
};
