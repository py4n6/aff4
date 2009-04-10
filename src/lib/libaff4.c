/** Public interface for aff4.
 */
#include "zip.h"
#include "aff4.h"

ZipFile open_volume(char *urn) {
  ZipFile result;
  char *filename;

  if(!strstr(urn, ":")) {
    filename = talloc_asprintf(NULL, "file://%s",urn);
  } else {
    filename = talloc_strdup(NULL, urn);
  }

  ClearError();
  result =(ZipFile)CONSTRUCT(DirVolume, ZipFile, super.Con, NULL, filename);
  if(!result)
    result = CONSTRUCT(ZipFile, ZipFile, Con, NULL, filename);

  PrintError();

  return result;
};

AFF4_HANDLE aff4_open(char **images) {
  // The opaque handle we actually return is a FileLikeObject.
  FileLikeObject result;
  char **i;
  // By default we try to open the stream named default
  char *stream="default";

  for(i=images; *i; i++) {
    // We try to load each image as a URL in itself:
    ZipFile tmp = open_volume(*i);
    if(tmp) {
      CALL(oracle, cache_return, (AFFObject)tmp);
    } else {
      // Its potentially a stream name:
      stream = *i;
    };
  };

  // Ok so now we loaded all volumes, we try to open a stream within
  // them:
  // Is the stream an explicit name?
  result = CALL(oracle, open, NULL, stream);
  if(!result) {
    // Maybe the stream is specified relative to one of the above
    // volumes?
    for(i=images; i; i++) {
      char buffer[BUFF_SIZE];
      snprintf(buffer, BUFF_SIZE, "%s/%s", *i, stream);
      result = CALL(oracle, open, NULL, stream);
      if(result) break;
    };
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
