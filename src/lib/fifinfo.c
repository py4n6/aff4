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
    if(CALL(zip, read_member, "foobar", &result, &length)<0) break;
  };

  gettimeofday(&new_time, NULL);
  diff = (new_time.tv_sec * 1000 + new_time.tv_usec/1000) -
    (epoch_time.tv_sec * 1000 + epoch_time.tv_usec/1000);
  printf("Decompressed foobar %d times in %d mseconds (%f)\n", 
	 TIMES,  diff,
	 ((float)diff)/TIMES);

  talloc_free(fd);
};

/** We try to create a new stream */
void test3() {
  // Open the file for reading
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, con, NULL, "new_test.zip", 'r');
  // Open the zip file
  FIFFile fif = CONSTRUCT(FIFFile, ZipFile, super.Con, fd, (FileLikeObject)fd);
  AFFFD image = CALL(fif, create_stream_for_writing, "default", "Image", NULL);
  
};

/** This tests the properties classes */
void test4() {
  Properties test = CONSTRUCT(Properties, Properties, Con, NULL);
  Properties i=NULL;

  CALL(test, add, "key", "value", 0);
  CALL(test, add, "key", "value", 0);
  CALL(test, add, "key", "something else", 0);
  CALL(test, add, "key2", "value", 0);
  CALL(test, parse, "key=foobar\nhello=world\n",100);

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
    length = read(out_fd, buffer, BUFF_SIZE * 10);
    if(length == 0) break;
    
    CALL(stream, write, buffer, length);
  };

  stream->close(stream);
  fiffile->super.close((ZipFile)fiffile);

  talloc_free(fiffile);
};


int main() {
  talloc_enable_leak_report_full();
  /*
  ClearError();
  test1();
  PrintError();

  ClearError();
  test2();
  PrintError();

  ClearError();
  test3();
  PrintError();

  ClearError();
  test4();
  PrintError();
  */
  ClearError();
  test5();
  PrintError();

  return 0;
};
