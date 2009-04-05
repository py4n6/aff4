/** This is an implementation of the directory volume - all blobs are
    stored as files here.

    You can always get the curl options required:

    curl --libcurl /tmp/test.c -X MKCOL http://localhost/webdav/test/
*/
#include "zip.h"

static AFFObject DirVolume_finish(AFFObject self) {
  DirVolume this = (DirVolume)self;
  AFFObject result = this->__super__->super.finish(self);

  // Correct our type:
  CALL(oracle, set, 
       URNOF(self), 	       /* Source URI */
       "aff2:type",            /* Attributte */
       "directory");           /* Value */

  return result;
};

static ZipFile DirVolume_Con(ZipFile self, char *fd_urn) {
  int len;
  char *text;
  // A temporary context
  char *ctx = talloc_strdup(self, "");
  Cache urn, first;

  // Make sure we have a URN
  CALL((AFFObject)self, Con, NULL);

  self->parent_urn = talloc_strdup(self, fd_urn);

  // We check that there is no URN there already:
  text = CALL(self, read_member, ctx, "__URN__", &len);
  if(text && len>0) {
    URNOF(self) = talloc_strdup(self, text);
  } else {
    CALL(self, writestr, "__URN__", ZSTRING_NO_NULL(URNOF(self)),
	 NULL, 0, 0);
  };

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
  urn = CALL(oracle->urn, get_item, URNOF(self));
  if(urn) {
    first = CALL(urn, get, "aff2:contains");
  };

  if(first) {
    Cache i;


    list_for_each_entry(i, &first->cache_list, cache_list) {
      char *filename = (char *)i->data;
      int filename_length;
      int properties_length;
      int len;

      if(!filename) continue;

      filename_length = strlen(filename);
      properties_length = strlen("properties");

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

  snprintf(buff, BUFF_SIZE, "%s/%s", self->parent_urn, escape_filename(filename));
  if(mode == 'w'){
    CALL(oracle, add, URNOF(self), "aff2:contains", filename);
  };

  result = (FileLikeObject)CALL(oracle, open, self, buff);
  return result;
};

// When we close we dump out the properties file of this directory
static void DirVolume_close(ZipFile self) {
  char *properties = CALL(oracle, export, URNOF(self));
  char buff[BUFF_SIZE];

  // We check that there is no URN there already:
  CALL(self, writestr, "properties", ZSTRING_NO_NULL(properties),
       NULL, 0, 0);
  talloc_free(properties);
};

static void DirVolume_writestr(ZipFile self, char *filename, 
		      char *data, int len, char *extra, int extra_len, 
		      int compression) {
  char buff[BUFF_SIZE];
  FileLikeObject fd;
  
  snprintf(buff, BUFF_SIZE, "%s/%s", self->parent_urn, escape_filename(filename));  
  CALL(oracle, add, URNOF(self), "aff2:contains", filename);

  fd = CALL(oracle, open, NULL, buff);
  if(!fd) return;
  CALL(fd, truncate, 0);
  CALL(fd, write, data, len);
  CALL(fd, close);
};

static char *DirVolume_read_member(ZipFile self, void *ctx,
				   char *filename,
				   int *length) {
  char buff[BUFF_SIZE];
  FileLikeObject fd;
  StringIO result = CONSTRUCT(StringIO, StringIO, Con, ctx);

  snprintf(buff, BUFF_SIZE, "%s/%s", self->parent_urn, escape_filename(filename));
  fd = (FileLikeObject)CALL(oracle, open, NULL, buff);
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
  return result->data;

 error:
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
