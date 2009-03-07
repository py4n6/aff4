#include "zip.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>


#define TEST_FILE "test.zip"

/** First test builds a new zip file from /bin/ls */
void test1() {
  // Make a new zip file
  ZipFile zip = (ZipFile)CONSTRUCT(FIFFile, ZipFile, super.Con, NULL, NULL);

  // Make a new file
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, Con, zip, TEST_FILE, 'w');
  FileLikeObject out_fd;

  // Create a new Zip volume for writing
  CALL(zip, create_new_volume, (FileLikeObject)fd);

  CALL(zip, writestr, "hello", ZSTRING_NO_NULL("hello world"), NULL, 0, 0);

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
    // Close the archive
    CALL(zip, close);
  };

  talloc_free(zip);
};

#define TIMES 1000

/** Try to create a new FIFFile.

When creating a new AFFObject we:

1) Ask the oracle to create it (prividing the class pointer).

2) Set all the required and optional parameters.

3) Call the finish method. If it succeeds we have a fully operational
object. If it fails (returns NULL), we may have failed to set some
parameters.

*/
void test1_5() {
  FileLikeObject fd = (FileLikeObject)CALL(oracle, create, (AFFObject *)&__FileBackedObject);
  FIFFile fiffile;

  // We now set critical parameters of the FileBackedObject
  fd->super.urn = "file://" TEST_FILE;
  
  // If this works the file is ok. 
  if(fd->super.finish((AFFObject)fd)) {
    // Now create a new AFF2 file on top of it
    fiffile = (FIFFile)CALL(oracle, create, (AFFObject *)&__FIFFile);
    CALL((AFFObject)fiffile, set_property, "aff2:stored", fd->super.urn);
    CALL((AFFObject)fiffile, set_property, "aff2:chunks_in_segment", "256");

    if(CALL((AFFObject)fiffile, finish)) {
      char buffer[BUFF_SIZE];
      int fd=open("/bin/ls",O_RDONLY);
      int length;
      FileLikeObject out_fd = CALL((ZipFile)fiffile, 
				   open_member, "foobar", 'w', NULL, 0, 
				   ZIP_DEFLATE);
      while(1) {
	length = read(fd, buffer, BUFF_SIZE);
	if(length == 0) break;
	
	CALL(out_fd, write, buffer, length);
      };
      CALL(out_fd,close);

      CALL((ZipFile)fiffile, writestr, "hello",
	   ZSTRING_NO_NULL("hello world"), NULL, 0, 0);

      CALL((ZipFile)fiffile,close);
    };
  };
  
  talloc_free(fd);
};

/** This tests the cache for reading zip members.

There are two steps: 

1) We open the zip file directly to populate the oracle.

2) We ask the oracle to open anything it knows about.

If you have a persistent oracle you dont need to use step 1 at all
since the information is already present.

*/
void test2() {
  // Open the file for reading
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, Con, NULL, TEST_FILE, 'r');
  // Open the zip file
  int i;
  struct timeval epoch_time;
  struct timeval new_time;
  int diff;
  Blob blob;

  // This is only needed to populate the oracle
  if(CONSTRUCT(FIFFile, ZipFile, super.Con, fd, URNOF(fd)))
    talloc_free(fd);

  // Now ask the resolver for the different files
  gettimeofday(&epoch_time, NULL);

  for(i=0;i<TIMES;i++) {
    blob = (Blob)CALL(oracle, open, fd, "hello");
    if(!blob) {
      RaiseError(ERuntimeError, "Error reading member");
      break;
    };

    CALL(oracle, cache_return, (AFFObject)blob);
  };

  gettimeofday(&new_time, NULL);
  printf("Resolving foobar produced **************\n%s\n******************\n", blob->data);
  diff = (new_time.tv_sec * 1000 + new_time.tv_usec/1000) -
    (epoch_time.tv_sec * 1000 + epoch_time.tv_usec/1000);
  printf("Decompressed foobar %d times in %d mseconds (%f)\n", 
	 TIMES,  diff,
	 ((float)diff)/TIMES);

  talloc_free(fd);
};

/** This test writes a two part AFF2 file.

First we ask the oracle to create a FileBackedObject then attach that
to a FIFFile volume. We then create an Image stream and attach that to
the FIFFile volume.

*/
void test_image_create() {
  FileLikeObject fd = (FileLikeObject)CALL(oracle, create, (AFFObject *)&__FileBackedObject);
  FIFFile fiffile;
  Image image;
  char buffer[BUFF_SIZE];
  int in_fd;
  int length;
  Link link;

  if(!fd) goto error;

  // We now set critical parameters of the FileBackedObject
  fd->super.urn = "file://" TEST_FILE;
  
  // Is it ok?
  if(!CALL((AFFObject)fd, finish))
    goto error;

  // Now build the FIF Volume:
  fiffile = (FIFFile)CALL(oracle, create, (AFFObject *)&__FIFFile);
  
  // Tell the fiffile that it should live in the FileBackedObject
  CALL((AFFObject)fiffile, set_property, "aff2:stored", fd->super.urn);

  // Is it ok?
  if(!CALL((AFFObject)fiffile, finish))
    goto error;

  // Now we need to create an Image stream
  image = (Image)CALL(oracle, create, (AFFObject *)&__Image);
  
  // Tell the image that it should be stored in the volume
  CALL((AFFObject)image, set_property, "aff2:stored", 
       ((AFFObject)fiffile)->urn);
  CALL((AFFObject)image, set_property, "aff2:chunks_in_segment", "256");
  
  // Is it ok?
  if(!CALL((AFFObject)image, finish))
    goto error;

  in_fd=open("/var/tmp/uploads/testimages/ntfs/ntfs1-gen2.dd",O_RDONLY);
  while(in_fd >= 0) {
    length = read(in_fd, buffer, BUFF_SIZE);
    if(length == 0) break;
    
    CALL((FileLikeObject)image, write, buffer, length);
  };

  CALL((FileLikeObject)image, close);

  // We want to make it easy to locate this image so we set up a link
  // to it:
  link = (Link)CALL(oracle, create, (AFFObject *)&__Link);
  // The link will be stored in this fiffile
  CALL((AFFObject)link, set_property, "aff2:stored", 
       ((AFFObject)fiffile)->urn);
  CALL(link, link, oracle, ((AFFObject)fiffile)->urn,
       ((AFFObject)image)->urn, "default");
  CALL((AFFObject)link, finish);

  CALL((ZipFile)fiffile, close);
  
 error:
  if(fd) talloc_free(fd);
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
  FIFFile fiffile;

  if(!fd) {
    RaiseError(ERuntimeError, "Unable to open file %s", TEST_FILE);
    return;
  };

  fiffile =CONSTRUCT(FIFFile, ZipFile, super.Con, fd, URNOF(fd));
  if(!fiffile) {
    RaiseError(ERuntimeError, "%s is not a zip file?", TEST_FILE);
    talloc_free(fd);
    return;
  };

  // We just put it in the cache anyway
  CALL(oracle, cache_return, (AFFObject)fiffile);

  image = (Image)CALL(oracle, open, NULL, link_name);
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

#if 0

void test3() {
  // Make a new zip file
  FIFFile fiffile = CONSTRUCT(FIFFile, ZipFile, super.Con, NULL, NULL);

  // Make a new file
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, Con,
				  fiffile, "test.00.zip", 'w');

  // Create a new properties object
  Properties props = CONSTRUCT(Properties, Properties, Con, fiffile);

  FileLikeObject stream;
  int out_fd = open("/bin/ls", O_RDONLY);
  char buffer[BUFF_SIZE * 10];
  int length;
  int volume_number=0;

  // Create a new Zip volume for writing
  fiffile->super.create_new_volume((ZipFile)fiffile, (FileLikeObject)fd);

  // For this test we have 2 chunks per segment
  CALL(props, add, "chunks_in_segment", "2", 0);

  // Make a new Image stream
  stream = (FileLikeObject)CALL(fiffile, create_stream_for_writing, "default","Image", props);
  while(stream) {
    length = read(out_fd, buffer, 10000);
    if(length == 0) break;
    
    CALL(stream, write, buffer, length);

    // If the FIF File gets too large we make a new volume:
    if(fd->super.size > 10000) {
      // Make a new fd
      volume_number ++ ;
      snprintf(buffer, BUFF_SIZE, "test.%02d.zip", volume_number);
      fd = CONSTRUCT(FileBackedObject, FileBackedObject, Con,
		     fiffile, buffer, 'w');
      
      fiffile->super.create_new_volume((ZipFile)fiffile, (FileLikeObject)fd);
    };
  };

  stream->close(stream);
  fiffile->super.close((ZipFile)fiffile);

  talloc_free(fiffile);
};

/** This tests the properties classes */
void test4() {
  Properties test = CONSTRUCT(Properties, Properties, Con, NULL);
  Properties i=NULL;

  CALL(test, add, "key", "value", 0);
  CALL(test, add, "key", "value", 0);
  CALL(test, add, "key", "something else", 0);
  CALL(test, add, "key2", "value", 0);
  CALL(test, parse, ZSTRING_NO_NULL("key=foobar\nhello=world\n"),100);

  while(1) {
    char *value=CALL(test, iter_next, &i, "key");
    if(!value) break;
    printf("Got value %s\n", value);
  };

  // Now dump the whole thing:
  printf("'%s'", CALL(test, str));	 

  talloc_free(test);
};

/** A little helper that copies a file into a fif archive */
FileLikeObject create_image(FIFFile fiffile, char *filename) {
  Properties props = CONSTRUCT(Properties, Properties, Con, fiffile);
  
  FileLikeObject stream;
  int out_fd = open(filename, O_RDONLY);
  char buffer[BUFF_SIZE * 10];
  int length;

  // Make a new Image stream
  stream = (FileLikeObject)CALL(fiffile, create_stream_for_writing, filename,
				"Image", props);
  while(stream) {
    length = read(out_fd, buffer, BUFF_SIZE*10);
    if(length == 0) break;
    
    CALL(stream, write, buffer, length);
  };

  CALL(stream, close);
  return stream;
};


/** Test the Image class */
void test5() {
  // Make a new zip file
  FIFFile fiffile = CONSTRUCT(FIFFile, ZipFile, super.Con, NULL, NULL);

  // Make a new file
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, Con,
				  fiffile, TEST_FILE, 'w');

  // Create a new properties object
  Properties props = CONSTRUCT(Properties, Properties, Con, fiffile);

  FileLikeObject stream;
  int out_fd = open("/bin/ls", O_RDONLY);
  char buffer[BUFF_SIZE * 10];
  int length;

  // Create a new Zip volume for writing
  fiffile->super.create_new_volume((ZipFile)fiffile, (FileLikeObject)fd);

  // Make a new Image stream
  stream = (FileLikeObject)CALL(fiffile, create_stream_for_writing, "default","Image", props);
  while(stream) {
    length = read(out_fd, buffer, BUFF_SIZE*10);
    if(length == 0) break;
    
    CALL(stream, write, buffer, length);
  };

  stream->close(stream);
  fiffile->super.close((ZipFile)fiffile);

  talloc_free(fiffile);
};

#define CHUNK_SIZE 32*1024

/** This tests the Map Image - we create an AFF file containing 3
    seperate streams and build a map. Then we read the map off and
    copy it into the output.
*/
void test7() {
  // Make a new zip file
  FIFFile fiffile = CONSTRUCT(FIFFile, ZipFile, super.Con, NULL, NULL);

  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, Con,
				  fiffile, TEST_FILE, 'w');
  FileLikeObject d0,d1,d2;
  MapDriver map;
  int blocksize = 64 * 1024;
  Properties props = CONSTRUCT(Properties, Properties, Con, fd);
  char buff[BUFF_SIZE];

  // Create a new Zip volume for writing
  fiffile->super.create_new_volume((ZipFile)fiffile, (FileLikeObject)fd);

  d0=create_image(fiffile, "images/d1.dd");
  d1=create_image(fiffile, "images/d2.dd");
  d2=create_image(fiffile, "images/d3.dd");

  // Now create a map stream:
  CALL(props, add, "image_period", "196608",0);
  CALL(props, add, "file_period", "393216",0);
  map = (MapDriver)CALL(fiffile, create_stream_for_writing, "combined", "Map", props);

  // Create the raid reassembly map
  CALL(map, add, 0,             0,             d1);
  CALL(map, add, 1 * blocksize, 0,             d0);
  CALL(map, add, 2 * blocksize, 1 * blocksize, d2);
  CALL(map, add, 3 * blocksize, 1 * blocksize, d1);
  CALL(map, add, 4 * blocksize, 2 * blocksize, d0);
  CALL(map, add, 5 * blocksize, 2 * blocksize, d2);
  
  map->super.super.size = d0->size * 2;
  map->save_map(map);
  map->super.super.close((FileLikeObject)map);

  talloc_free(fiffile);
};

#endif

int main() {
  talloc_enable_leak_report_full();

  /*
  AFF2_Init();
  ClearError();
  test1_5();
  PrintError();

  ClearError();
  test1();
  PrintError();

  AFF2_Init();
  ClearError();
  test2();
  PrintError();
  */

  AFF2_Init();
  ClearError();
  test_image_create();
  PrintError();

  AFF2_Init();
  ClearError();
  test_image_read();
  PrintError();
  
/*
  
  ClearError();
  printf("\n*******************\ntest 5\n********************\n");
  test5();
  PrintError();


  ClearError();
  printf("\n*******************\ntest 6\n********************\n");
  test6();
  PrintError();

  ClearError();
  printf("\n*******************\ntest 7\n********************\n");
  test7();
  PrintError();
  */
  return 0;
};
