#include "fif.h"
#include "class.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>


/** First test builds a new zip file from /bin/ls */
void test1() {
  // Make a new zip file
  ZipFile zip = CONSTRUCT(ZipFile, ZipFile, Con, NULL, NULL);

  // Make a new file
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, con, zip, "new_test.zip", 'w');
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

/** This tests the cache for reading zip members */
void test2() {
  // Open the file for reading
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, con, NULL, "new_test.zip", 'r');
  // Open the zip file
  ZipFile zip = CONSTRUCT(ZipFile, ZipFile, Con, fd, (FileLikeObject)fd);

  char *result=NULL;
  int length=0;
  int i;
  struct timeval epoch_time;
  struct timeval new_time;
  int diff;
  
  gettimeofday(&epoch_time, NULL);

  for(i=0;i<TIMES;i++) {
    result = CALL(zip, read_member, "foobar", &length);
    if(!result) {
      RaiseError(ERuntimeError, "Error reading member");
      break;
    };
  };

  gettimeofday(&new_time, NULL);
  diff = (new_time.tv_sec * 1000 + new_time.tv_usec/1000) -
    (epoch_time.tv_sec * 1000 + epoch_time.tv_usec/1000);
  printf("Decompressed foobar %d times in %d mseconds (%f)\n", 
	 TIMES,  diff,
	 ((float)diff)/TIMES);

  talloc_free(fd);
};

/** This test writes a two part AFF2 file */
void test3() {
  // Make a new zip file
  FIFFile fiffile = CONSTRUCT(FIFFile, ZipFile, super.Con, NULL, NULL);

  // Make a new file
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, con,
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
      fd = CONSTRUCT(FileBackedObject, FileBackedObject, con,
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

/** Test the Image class */
void test5() {
  // Make a new zip file
  FIFFile fiffile = CONSTRUCT(FIFFile, ZipFile, super.Con, NULL, NULL);

  // Make a new file
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, con,
				  fiffile, "new_test.zip", 'w');

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
    length = read(out_fd, buffer, 100);
    if(length == 0) break;
    
    CALL(stream, write, buffer, length);
  };

  stream->close(stream);
  fiffile->super.close((ZipFile)fiffile);

  talloc_free(fiffile);
};

#define CHUNK_SIZE 32*1024

/** Test reading of the Image stream */
void test6() {
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, con, NULL, "new_test.zip", 'r');
  FIFFile fiffile;
  AFFFD image;
  int outfd;
  char buff[CHUNK_SIZE];
  int length;

  if(!fd) goto error;

  // Make a new zip file
  fiffile  = CONSTRUCT(FIFFile, ZipFile, super.Con, fd, (FileLikeObject)fd);
  if(!fiffile) goto error;

  image = CALL(fiffile, open_stream, "default");
  if(!image) goto error;

  outfd = creat("output.dd", 0644);
  if(outfd<0) goto error;

  while(1) {
    length = image->super.read((FileLikeObject)image, buff, 1);
    if(length<=0) break;

    write(outfd, buff, length);
  };

  close(outfd);

  exit(0);
 error:
  talloc_free(fd);
};


int main() {
  talloc_enable_leak_report_full();

  ClearError();
  test1();
  PrintError();

  ClearError();
  test2();
  PrintError();

  ClearError();
  test3();
  PrintError();
  /*
  ClearError();
  test4();
  PrintError();

  ClearError();
  test5();
  PrintError();

  ClearError();
  test6();
  PrintError();
  */
  return 0;
};
