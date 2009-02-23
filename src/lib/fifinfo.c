#include "fif.h"
#include "class.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main() {
  ZipFile zip = CONSTRUCT(ZipFile, ZipFile, Con, NULL, NULL);
  FileBackedObject fd = CONSTRUCT(FileBackedObject, FileBackedObject, con, zip, "new_test.zip", 'w');
  FileBackedObject out_fd;

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

    CALL(out_fd, close);
    CALL(zip, close);
  };

  PrintError();
};
