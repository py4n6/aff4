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

  PrintError();
  talloc_free(zip);
};

#define TIMES 1000

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
  PrintError();
  talloc_free(fd);
};

int main() {
  ClearError();
  test1();
  ClearError();
  test2();
};
