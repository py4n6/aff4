/** Public interface for aff4.
 */
#include "zip.h"
#include "aff4.h"

ZipFile open_volume(char *urn, char mode) {
  ZipFile result=NULL;
  char *filename;

  if(!strstr(urn, ":")) {
    filename = talloc_asprintf(NULL, "file://%s",urn);
  } else {
    filename = talloc_strdup(NULL, urn);
  }

  ClearError();

  // Try to pull it from cache
  {
    char *volume = CALL(oracle, resolve, filename, AFF4_CONTAINS);
    if(volume)
      result = (ZipFile)CALL(oracle, open, NULL, volume, mode);
  };

  // Nope - we need to make it. Try a directory volume first:
  if(!result)
    result =(ZipFile)CONSTRUCT(DirVolume, ZipFile, super.Con, NULL, filename, mode);

  // Nope - maybe a ZipFile?
  if(!result)
    result = CONSTRUCT(ZipFile, ZipFile, Con, NULL, filename, mode);

  PrintError();

  return result;
};

/** Opens an AFF4 file for reading */
AFF4_HANDLE aff4_open(char **images) {
  // The opaque handle we actually return is a FileLikeObject.
  FileLikeObject result=NULL;
  char **i;
  // By default we try to open the stream named default
  char *stream="default";

  for(i=images; *i; i++) {
    // We try to load each image as a URL in itself:
    FileLikeObject fd= (FileLikeObject)CALL(oracle, open, NULL, *i, 'r');
    ZipFile tmp;

    // This is actually a stream they specified
    if(fd && ISSUBCLASS(fd, FileLikeObject)) {
      stream = URNOF(fd);
    };

    if(fd) {
      CALL(oracle, cache_return, (AFFObject)fd);
    };
    
    tmp = open_volume(*i, 'r');
    if(tmp) {
      CALL(oracle, cache_return, (AFFObject)tmp);
    } else {
      // Its potentially a stream name:
      stream = *i;
    };
  };

  // Ok so now we loaded all volumes, we try to open a stream within
  // them:
  // Is the stream a fully qualified name?
  if(!strstr(stream, FQN)) {
    // Maybe the stream is specified relative to one of the above
    // volumes?
    LogWarnings("Trying to open stream %s relative to all volumes", stream);
    for(i=images; *i; i++) {
      char buffer[BUFF_SIZE];
      ZipFile tmp = open_volume(*i, 'r');

      if(tmp) {
	snprintf(buffer, BUFF_SIZE, "%s/%s", URNOF(tmp), stream);
	LogWarnings("Trying %s", buffer);
	CALL(oracle, cache_return, (AFFObject)tmp);

	result = (FileLikeObject)CALL(oracle, open, NULL, buffer, 'r');
	if(result) {
	  break;
	};
      };
    };
  } else {
    result = (FileLikeObject)CALL(oracle, open, NULL, stream, 'r');
  };


  if(result) {
    LogWarnings("Using %s as the stream name", URNOF(result));
  } else {
    LogWarnings("No suitable stream found");
  };

  return result;
};

void aff4_seek(AFF4_HANDLE self, uint64_t offset, int whence) {
  FileLikeObject result = (FileLikeObject)self;

  if(result) {
    CALL(result, seek, offset, whence);
  };
};

uint64_t aff4_tell(AFF4_HANDLE self) {
  FileLikeObject result = (FileLikeObject)self;

  return CALL(result, tell);
};

int aff4_read(AFF4_HANDLE self, char *buf, int len) {
  FileLikeObject result = (FileLikeObject)self;

  return CALL(result, read, buf, len);
};

void aff4_close(AFF4_HANDLE self) {
  // Just give the handle back to the oracle so it may be reused if
  // needed.
  CALL(oracle, cache_return, (AFFObject)self);
};
