/** This file implements the basic file handling code. We allow
    concurrent read/write.

    FIXME: The FileBackedObject needs to be tailored for windows.
*/
#include "aff4_internal.h"


/** Implementation of FileBackedObject.

    FileBackedObject is a FileLikeObject which uses a real file to back itself.

    NOTE that file backed objects do not actually have any information stored in the
    resolver - they get all their information directly from the backing file.
*/

/** Only close the file when we are actually freed */
static int FileBackedObject_destructor(void *self) {
  FileBackedObject this=(FileBackedObject)self;

  close(this->fd);

  return 0;
};


/** Note that files we create will always be escaped using standard
    URN encoding.
*/
static int FileBackedObject_AFFObject_finish(AFFObject this) {
  FileBackedObject self = (FileBackedObject)this;
  int flags;

  // Make sure that the urn passed has a file scheme
  if(!this->urn->parser || (strlen(this->urn->parser->scheme) > 0 &&
                            strcmp("file", this->urn->parser->scheme))) {
    RaiseError(ERuntimeError, "%s must be called with a file:// scheme", NAMEOF(self));
    goto error;
  };

  if(this->mode == 'r') {
    flags = O_RDONLY | O_BINARY;
  } else {
    flags = O_CREAT | O_RDWR | O_BINARY;
    // Try to create leading directories
    //_mkdir(dirname(urn->parser->query));
  };

  // Now try to create the file within the new directory
  self->fd = open(this->urn->parser->query, flags, S_IRWXU | S_IRWXG | S_IRWXO);
  if(self->fd < 0){
    RaiseError(EIOError, "Can't open %s (%s)", this->urn->parser->query, strerror(errno));
    goto error;

  } else {
    // Add a destructor to this object
    talloc_set_destructor((void *)self, FileBackedObject_destructor);
  };

  return 1;

 error:
  return 0;
};


/* Only support a limited set of attributes for FileBackedObjects - virtualized
 * from the backing file.
 */
static RDFValue FileBackedObject_resolve(AFFObject this, void *ctx, char *attribute) {
  XSDInteger result = NULL;
  FileBackedObject self = (FileBackedObject)this;

  if(!strcmp(attribute, AFF4_SIZE)) {
    result = new_XSDInteger(ctx);
    result->value = lseek(self->fd, 0, SEEK_END);
  };

  return (RDFValue)result;
};


static int FileLikeObject_truncate(FileLikeObject self, uint64_t offset) {
  self->readptr = min(offset, self->readptr);
  return offset;
};

static uint64_t FileLikeObject_seek(FileLikeObject self, int64_t offset, int whence) {
  if(whence==SEEK_SET) {
    self->readptr = offset;
  } else if(whence==SEEK_CUR) {
    self->readptr += offset;
    if(self->readptr<0) self->readptr=0;
  } else if(whence==SEEK_END) {
    XSDInteger size = (XSDInteger)CALL((AFFObject)self, resolve, NULL, AFF4_SIZE);

    self->readptr = size->value + offset;
    talloc_free(size);
  };

  if(self->readptr < 0) {
    self->readptr=0;
  };

  return self->readptr;
};

static int FileLikeObject_readline(FileLikeObject self, char *buffer, unsigned int length) {
  int result;
  int i;
  uint64_t offset = self->readptr;

  result = CALL(self, read, buffer, length);
  if(result >0) {
    for(i=0; i<result-1; i++) {
      switch(buffer[i]) {
      case '\r':
        if(buffer[i+1] != '\n') break;
        offset++;
      case '\n':
        buffer[i]=0;
        self->readptr = offset + i+1;
        return i;
      default:
        break;
      };
    };
  };

  return result;
};

/**
    read some data from our file into the buffer (which is assumed to
    be large enough).
**/
static int FileBackedObject_read(FileLikeObject self, char *buffer, unsigned int length) {
  FileBackedObject this = (FileBackedObject)self;
  int result;

  lseek(this->fd,self->readptr,0);
  result = read(this->fd, buffer, length);
  if(result < 0) {
    RaiseError(EIOError, "Unable to read from %s (%s)", URNOF(self), strerror(errno));
    return -1;
  };

  self->readptr += result;

  return result;
};

static int FileBackedObject_write(FileLikeObject self, char *buffer, unsigned int length) {
  FileBackedObject this = (FileBackedObject)self;
  int result;

  if(length == 0) return 0;

  lseek(this->fd,self->readptr,SEEK_SET);
  result = write(this->fd, buffer, length);
  if(result < 0) {
    RaiseError(EIOError, "Unable to write to %s (%s)", URNOF(self)->value, strerror(errno));
  };

  self->readptr += result;

  return result;
};

static uint64_t FileLikeObject_tell(FileLikeObject self) {
  return self->readptr;
};

static int FileLikeObject_close(AFFObject self) {
  // Make sure we are no longer dirty - this is used to keep track of
  // outstanding objects which are not closed when a volume is closed.
  CALL(self->resolver, del, URNOF(self), AFF4_VOLATILE_DIRTY);

  return SUPER(AFFObject, AFFObject, close);
};

static uint64_t FileBackedObject_seek(FileLikeObject self, int64_t offset, int whence) {
  FileBackedObject this = (FileBackedObject)self;

  int64_t result = lseek(this->fd, offset, whence);

  if(result < 0) {
    DEBUG_OBJECT("Error seeking %s\n", strerror(errno));
    result = 0;
  };

  self->readptr = result;
  return result;
};

static XSDString FileLikeObject_get_data(FileLikeObject self, void *ctx) {
  XSDString data;
  XSDInteger size = (XSDInteger)CALL((AFFObject)self, resolve, ctx, AFF4_SIZE);

  if(size->value > MAX_CACHED_FILESIZE)
    goto error;

  CALL(self, seek, 0,0);
  data = CONSTRUCT(XSDString, RDFValue, Con, ctx);
  data->length = size->value;
  data->value = talloc_size(self, size->value + BUFF_SIZE);

  memset(data->value, 0, size->value + BUFF_SIZE);
  CALL(self, read, data->value, size->value);

  talloc_free(size);

  return data;

 error:
  talloc_free(size);
  return NULL;
};

VIRTUAL(FileLikeObject, AFFObject) {
     VMETHOD(seek) = FileLikeObject_seek;
     VMETHOD(tell) = FileLikeObject_tell;
     VMETHOD_BASE(AFFObject, close) = FileLikeObject_close;
     VMETHOD(truncate) = FileLikeObject_truncate;
     VMETHOD(get_data) = FileLikeObject_get_data;
     VMETHOD(readline) = FileLikeObject_readline;
} END_VIRTUAL

static int FileBackedObject_truncate(FileLikeObject self, uint64_t offset) {
  FileBackedObject this=(FileBackedObject)self;

  ftruncate(this->fd, offset);
  return SUPER(FileLikeObject, FileLikeObject, truncate, offset);
};

/** A file backed object extends FileLikeObject */
VIRTUAL(FileBackedObject, FileLikeObject) {
  VMETHOD_BASE(AFFObject, finish) = FileBackedObject_AFFObject_finish;
  VMETHOD_BASE(AFFObject, dataType) = AFF4_FILE;
  VMETHOD_BASE(AFFObject, resolve) = FileBackedObject_resolve;

  VMETHOD_BASE(FileLikeObject, read) = FileBackedObject_read;
  VMETHOD_BASE(FileLikeObject, write) = FileBackedObject_write;
  VMETHOD_BASE(FileLikeObject, seek) = FileBackedObject_seek;
  VMETHOD_BASE(FileLikeObject, truncate) = FileBackedObject_truncate;
} END_VIRTUAL;


AFF4_MODULE_INIT(A000_file) {
  register_type_dispatcher(AFF4_FILE, (AFFObject *)GETCLASS(FileBackedObject));
};
