#include "fif.h"
#include "zip.h"
#include "time.h"
#include <uuid/uuid.h>
#include <libgen.h>

uint64_t parse_int(char *string) {
  char *endptr;
  uint64_t result;

  result = strtoll(string, &endptr, 0);
  switch(*endptr) {
  case 's':
    result *= 512;
    break;
  case 'K':
  case 'k':
    result *= 1024;
    break;
  case 'm':
  case 'M':
    result *= 1024*1024;
    break;
  case 'g':
  case 'G':
    result *= 1024*1024*1024;
    break;
  default:
    break;
  };

  return result;
};

static int FIFFile_destructor(void *self) {
  ZipFile this = (ZipFile)self;
  CALL(this, close);

  return 0;
};

static ZipFile FIFFile_Con(ZipFile self, FileLikeObject fd) {
  FIFFile this = (FIFFile)self;
  Cache i;

  // Open the file now
  self = this->__super__->Con(self, fd);

  // Now tell the resolver about everything we know:
  list_for_each_entry(i, &self->zipinfo_cache->cache_list, cache_list) {
    ZipInfo zip = (ZipInfo)i->data;

    CALL(oracle, add,
	 zip->filename,
	 "aff2:location",
	 self->super.urn);

    CALL(oracle, add,
	 zip->filename,
	 "aff2:type",
	 "blob");

  };

  return self;
};

/** This method gets called whenever the CD parsing finds a new
    archive member. We parse any properties files we find.
*/
static void FIFFile_add_zipinfo_to_cache(ZipFile self, ZipInfo zip) {
  FIFFile this = (FIFFile)self;
  int filename_length = strlen(zip->filename);
  int properties_length = strlen("properties");
  
  // Call our baseclass
  this->__super__->add_zipinfo_to_cache(self,zip);

  // We identify streams by their filename ending with "properties"
  // and parse out their properties:
  if(filename_length >= properties_length && 
     !strcmp("properties", zip->filename + filename_length - properties_length)) {
    int len;
    char *text;
    int i,j,k;
    // Make our own local copy so we can modify it (the original is
    // cached and doesnt belong to us).
    char *tmp;
    char *tmp_text;
    char *source;
    char *attribute;
    char *value;

    text = CALL(self, read_member, zip->filename, &len);
    if(!text) return;

    tmp = talloc_memdup(self, text, len+1);
    tmp[len]=0;
    tmp_text = tmp;

    // Find the next line:
    while((i=strcspn(tmp_text, "\r\n"))) {
      tmp_text[i]=0;
      
      // Locate the =
      j=strcspn(tmp_text,"=");
      if(j==i) goto exit;
      
      tmp_text[j]=0;
      value = tmp_text + j + 1;

      // Locate the space
      k=strcspn(tmp_text," ");
      if(k==j) {
	// No absolute URN specified, we use the current filename:
	source = talloc_strdup(tmp, zip->filename);
	source = dirname(source);
	attribute = tmp_text;
      } else {
	source = tmp_text;
	attribute = tmp_text + k+1;
      };
      tmp_text[k]=0;

      /*
      if(strlen(attribute)<4 || memcmp(attribute, ZSTRING_NO_NULL("aff2:"))) {
	attribute = talloc_asprintf(tmp, "aff2:%s",attribute);
      }
      */

      // Try to recognise a statement like URN stored . (URN stored
      // here) to find the current volumes UUID.
      if((!strcmp(value,".") || !strcmp(value,self->fd->super.urn)) && 
	  !strcmp(attribute,"aff2:stored")) {
	value = self->fd->super.urn;
	self->super.urn = talloc_strdup(self, source);
      };

      // Now add to the global resolver (These will all be possibly
      // stolen).
      CALL(oracle, add, 
	   talloc_strdup(tmp, source),
	   talloc_strdup(tmp, attribute),
	   talloc_strdup(tmp, value));

      // Move to the next line
      tmp_text = tmp_text + i+1;
    };
    
  printf("Found property file %s\n%s", zip->filename, text);

 exit:
  talloc_free(tmp);
  };

};

static void FIFFile_close(ZipFile self) {
  // Force our properties to be written
  FIFFile this = (FIFFile)self;
  char *str;
  if(self->_didModify) {
    // Export the volume properties here
    str = CALL(oracle, export, self->super.urn);
    if(str) {
      CALL(self, writestr, "properties", ZSTRING_NO_NULL(str),
	   NULL,0, 0);
      talloc_free(str);
    };
  };

  // Call our base class:
  this->__super__->close(self);
};

/** This is the constructor which will be used when we get
    instantiated as an AFFObject.
*/
AFFObject FIFFile_AFFObject_Con(AFFObject self, char *urn) {
  FIFFile this = (FIFFile)self;

  if(urn) {
    // Ok, we need to create ourselves from a URN. We need a
    // FileLikeObject first. We ask the oracle what object should be
    // used as our underlying FileLikeObject:
    char *url = CALL(oracle, resolve, urn, "aff2:stored");
    FileLikeObject fd;

    // We have no idea where we are stored:
    if(!url) {
      RaiseError(ERuntimeError, "Can not find the storage for Volume %s", urn);
      goto error;
    };

    // Ask the oracle for the file object
    fd = (FileLikeObject)CALL(oracle, open, url);
    if(!fd) goto error;

    // Call our other constructor to actually read this file:
    self = (AFFObject)this->super.Con((ZipFile)this, fd);
  } else {
    // Call ZipFile's AFFObject constructor.
    this->__super__->super.Con(self, urn);
  };

  return self;
 error:
  talloc_free(self);
  return NULL;
};

static AFFObject FIFFile_finish(AFFObject self) {
  FIFFile this = (FIFFile)self;
  char *file_urn = CALL(oracle, resolve, self->urn, "aff2:stored");
  FileLikeObject fd;

  if(!file_urn) {
    RaiseError(ERuntimeError, "Volume %s has no aff2:stored property?", self->urn);
    return NULL;
  };

  // Try to open our underlying FileLikeObject
  fd = (FileLikeObject)CALL(oracle, create, (AFFObject *)&__FileBackedObject);
  fd->super.urn = file_urn;
  
  // Did it work?
  if(CALL((AFFObject)fd, finish)) {
    CALL(oracle, add, 
	 ((AFFObject)self)->urn,	       /* Source URI */
	 "aff2:type",   /* Attributte */
	 "volume");  /* Value */

    // Call our base class to actually create the new volume:
    this->__super__->Con((ZipFile)this, NULL);
    CALL((ZipFile)this, create_new_volume, fd);
  };
  
  return self;
};

VIRTUAL(FIFFile, ZipFile)
     VMETHOD(super.close) = FIFFile_close;
     VMETHOD(super.add_zipinfo_to_cache) = FIFFile_add_zipinfo_to_cache;

// There are two constructors:
     VMETHOD(super.super.Con) = FIFFile_AFFObject_Con;
     VMETHOD(super.super.finish) = FIFFile_finish;
     VMETHOD(super.Con) = FIFFile_Con;
END_VIRTUAL
