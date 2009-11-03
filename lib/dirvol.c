/** This is an implementation of the directory volume - all segments are
    stored as files here.

*/
#include "zip.h"
#include <libgen.h>

static AFFObject DirVolume_finish(AFFObject self) {
  DirVolume this = (DirVolume)self;
  AFFObject result = this->__super__->super.finish(self);

  // Correct our type:
  CALL(oracle, set, 
       URNOF(self), 	       /* Source URI */
       AFF4_TYPE,            /* Attributte */
       AFF4_DIRECTORY_VOLUME,
       RESOLVER_DATA_STRING);

  return result;
};

static ZipFile DirVolume_Con(ZipFile self, char *fd_urn, char mode) {
  int len;
  char *text;
  // A temporary context
  char *ctx = talloc_strdup(self, "");
  Cache urn, first;

  // Make sure we have a URN
  CALL((AFFObject)self, Con, NULL, mode);

  // We check that there is no URN there already:
  PrintError();
  text = CALL(self, read_member, ctx, "__URN__", &len);
  if(text && len>0) {
    URNOF(self) = talloc_strdup(self, text);
  } else if(mode=='w') {
    // We are in writing mode - but we dont already have a __URN__
    // file - make one:
    if(CALL(self, writestr, "__URN__", ZSTRING_NO_NULL(URNOF(self)),
	    NULL, 0, 0)<0) {
      goto error;
    };
  } else {
    // We are in reading mode and we cant find the __URN__ file - quit
    ClearError();
    RaiseError(ERuntimeError, "%s not a valid directory volume - no __URN__", fd_urn)

    goto error;
  };
  ClearError();
  
  /** Ok - now parse all the properties files. We do this by first
      parsing the properties file at the volume layer, then iterating
      over all the aff2:contained attributes in it to find other
      properties files within the directory.
  */
  text = CALL(self, read_member, self, "properties", &len);
  if(text) {
    CALL(oracle, parse, URNOF(self), text, len);
  };
  
  // Now iterate over all the contained members in this directory:
  //FIXME?
  //  urn = CALL(oracle->urn, get_item, URNOF(self));
  if(urn) {
    first = CALL(urn, get, AFF4_CONTAINS);
  };
  
  if(first) {
    Cache i;
    
    list_for_each_entry(i, &first->hash_list, hash_list) {
      char *filename = (char *)i->data;
      int filename_length;
      int properties_length;
      int len;
      
      if(!filename) continue;
      
      filename_length = strlen(filename);
      properties_length = strlen("properties");
      
      // Record the segment objects:
      CALL(oracle, set, filename, AFF4_TYPE, AFF4_SEGMENT, RESOLVER_DATA_STRING);
      CALL(oracle, set, filename, AFF4_STORED, URNOF(self), RESOLVER_DATA_URN);
      
      // We identify streams by their filename ending with "properties"
      // and parse out their properties:
      if(filename_length >= properties_length && 
	 !strcmp("properties", filename + filename_length - properties_length)) {
	char *text = CALL(self, read_member, self, filename, &len);
	char *context = dirname(talloc_strdup(ctx, filename));
	
	//printf("Found property file %s\n%s", filename, text);
	
	if(text) {
	  CALL(oracle, parse, context, text, len);
	};
      };
    };  
  };
  
  talloc_free(ctx);
  return self;
    
 error:
  talloc_free(self);
  return NULL;
};

static FileLikeObject DirVolume_open_member(ZipFile self, char *filename, char mode,
				   char *extra, uint16_t extra_field_len,
				   int compression) {
  char buff[BUFF_SIZE];
  FileLikeObject result;
  char *escaped_filename = escape_filename(self, filename, 0);
  char storage_urn[BUFF_SIZE];
  
  if(!CALL(oracle, resolve2, URNOF(self), AFF4_STORED, storage_urn, 
	   BUFF_SIZE, RESOLVER_DATA_URN)) {
    RaiseError(ERuntimeError, "Can not find the storage for Volume %s", URNOF(self));
    return NULL;
  };

  snprintf(buff, BUFF_SIZE, "%s/%s", storage_urn, escaped_filename);
  result = (FileLikeObject)CALL(oracle, open, buff, mode);
  
  if(result && mode == 'w'){
    CALL(oracle, add, URNOF(self), AFF4_CONTAINS, filename, RESOLVER_DATA_URN, 1);
  };

  talloc_free(escaped_filename);
  return result;
};

// When we close we dump out the properties file of this directory
static void DirVolume_close(ZipFile self) {
  char *properties = CALL(oracle, export_urn, URNOF(self), URNOF(self));

  // We check that there is no URN there already:
  CALL(self, writestr, "properties", ZSTRING_NO_NULL(properties),
       NULL, 0, 0);
  talloc_free(properties);
};

static int DirVolume_writestr(ZipFile self, char *filename, 
		      char *data, int len, char *extra, int extra_len, 
		      int compression) {
  char buff[BUFF_SIZE];
  FileLikeObject fd;
  char *escaped_filename = escape_filename(self, filename, 0);
  char storage_urn[BUFF_SIZE];
  
  if(!CALL(oracle, resolve2, URNOF(self), AFF4_STORED, storage_urn, 
	   BUFF_SIZE, RESOLVER_DATA_URN)) {
    RaiseError(ERuntimeError, "Can not find the storage for Volume %s", URNOF(self));
    return 0;
  };

  snprintf(buff, BUFF_SIZE, "%s/%s", storage_urn, escaped_filename);  
  
  fd = (FileLikeObject)CALL(oracle, open, buff, 'w');
  if(!fd) {
    talloc_free(escaped_filename);
    return -1;
  };

  CALL(oracle, add, URNOF(self), AFF4_CONTAINS, filename, RESOLVER_DATA_URN, 1);
  CALL(fd, truncate, 0);
  len = CALL(fd, write, data, len);
  CALL(fd, close);

  talloc_free(escaped_filename);
  return len;
};

static char *DirVolume_read_member(ZipFile self, void *ctx,
				   char *filename,
				   int *length) {
  char buff[BUFF_SIZE];
  FileLikeObject fd;
  StringIO result = CONSTRUCT(StringIO, StringIO, Con, ctx);
  char *escaped_filename = escape_filename(self, filename, 0);
  char storage_urn[BUFF_SIZE];
  
  if(!CALL(oracle, resolve2, URNOF(self), AFF4_STORED, storage_urn, 
	   BUFF_SIZE, RESOLVER_DATA_URN)) {
    RaiseError(ERuntimeError, "Can not find the storage for Volume %s", URNOF(self));
    return NULL;
  };

  snprintf(buff, BUFF_SIZE, "%s/%s", storage_urn, escaped_filename);
  fd = (FileLikeObject)CALL(oracle, open, buff, 'r');
  if(!fd) {
    goto error;
  };

  CALL(fd, seek, 0, 0);
  // Read the entire file into memory
  while(1) {
    int len = CALL(fd, read, buff, BUFF_SIZE);
    if(len <= 0) break;
    CALL(result, write, buff, len);
  };

  CALL(oracle, cache_return, (AFFObject)fd);
  // NULL terminate the buffer
  buff[0]=0;
  CALL(result, write, buff, 1);
  *length = result->size-1;

  talloc_free(escaped_filename);
  return result->data;

 error:
  talloc_free(escaped_filename);
  talloc_free(result);
  return NULL;
};

VIRTUAL(DirVolume, ZipFile)
     VMETHOD(super.super.finish) = DirVolume_finish;
     VMETHOD(super.open_member) = DirVolume_open_member;
     VMETHOD(super.read_member) = DirVolume_read_member;
     VMETHOD(super.writestr) = DirVolume_writestr;
     VMETHOD(super.Con) = DirVolume_Con;
     VMETHOD(super.close) = DirVolume_close;
END_VIRTUAL
