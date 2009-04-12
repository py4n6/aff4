/** This file implements the basic Zip handling code. We allow
    concurrent read/write.

    FIXME: The FileBackedObject needs to be tailored for windows.
*/
#include "zip.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "list.h"
#include <unistd.h>
#include <zlib.h>
#include <time.h>
#include <libgen.h>

/** Implementation of FileBackedObject.

FileBackedObject is a FileLikeObject which uses a real file to back
itself.
*/
/** Note that files we create will always be escaped using standard
    URN encoding.
*/
static FileBackedObject FileBackedObject_Con(FileBackedObject self, 
					     char *filename, char mode) {
  int flags;
  char *buffer;

  if(mode == 'r') {
    flags = O_RDONLY | O_BINARY;
  } else {
    flags = O_CREAT | O_RDWR | O_BINARY;
    buffer = escape_filename(filename);
    // Try to create leading directories
    _mkdir(dirname(buffer));
  };

  // Now try to create the file within the new directory
  buffer = escape_filename(filename);
  self->fd = open(buffer, flags, S_IRWXU | S_IRWXG | S_IRWXO);
  if(self->fd<0){
    RaiseError(EIOError, "Can't open %s (%s)", filename, strerror(errno));
    goto error;
  };

  // Work out what the file size is:
  self->super.size = lseek(self->fd, 0, SEEK_END);

  // Set our urn
  URNOF(self) = talloc_asprintf(self, "file://%s", filename);

  return self;

 error:
  talloc_free(self);
  return NULL;
};

// This is the low level constructor for FileBackedObject.
static AFFObject FileBackedObject_AFFObject_Con(AFFObject self, char *urn, char mode) {
  FileBackedObject this = (FileBackedObject)self;

  self->mode = mode;

  if(urn) {
    // If the urn starts with file:// we open the filename, otherwise
    // we try to open the actual file itself:
    if(!memcmp(urn, ZSTRING_NO_NULL("file://"))) {
      self = (AFFObject)this->Con(this, urn + strlen("file://"), mode);
    } else {
      self = (AFFObject)this->Con(this, urn, mode);
    };
  } else {
    self = this->__super__->super.Con((AFFObject)this, urn, mode);
  };

  return self;
};

static int FileLikeObject_truncate(FileLikeObject self, uint64_t offset) {
  self->size = offset;
  self->readptr = min(self->size, self->readptr);
  return offset;
};

static uint64_t FileLikeObject_seek(FileLikeObject self, int64_t offset, int whence) {
  if(whence==SEEK_SET) {
    self->readptr = offset;
  } else if(whence==SEEK_CUR) {
    self->readptr += offset;
    if(self->readptr<0) self->readptr=0;
  } else if(whence==SEEK_END) {
    self->readptr = self->size + offset;
  };

  if(self->readptr < 0) {
    self->readptr=0;

  } else if(self->readptr > self->size) {
    self->readptr = self->size;
  };

  return self->readptr;
};
  
/** 
    read some data from our file into the buffer (which is assumed to
    be large enough).
**/
static int FileBackedObject_read(FileLikeObject self, char *buffer, unsigned long int length) {
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

static int FileBackedObject_write(FileLikeObject self, char *buffer, unsigned long int length) {
  FileBackedObject this = (FileBackedObject)self;
  int result;

  lseek(this->fd,self->readptr,0);
  result = write(this->fd, buffer, length);
  if(result < 0) {
    RaiseError(EIOError, "Unable to write to %s (%s)", URNOF(self), strerror(errno));
  };

  self->readptr += result;

  self->size = max(self->size, self->readptr);

  return result;
};

static uint64_t FileLikeObject_tell(FileLikeObject self) {
  return self->readptr;
};

static void FileLikeObject_close(FileLikeObject self) {
  //talloc_free(self);
};

static void FileBackedObject_close(FileLikeObject self) {
  FileBackedObject this=(FileBackedObject)self;

  close(this->fd);
  talloc_free(self);
};

static AFFObject FileBackedObject_finish(AFFObject self) {
  FileBackedObject this = (FileBackedObject)self;
  char *urn = self->urn;
 
  if(!memcmp(urn, ZSTRING_NO_NULL("file://"))) {
    return (AFFObject)this->Con(this, urn + strlen("file://"), 'w');
  } else {
    return (AFFObject)this->Con(this, urn, 'w');
  };
};

VIRTUAL(FileLikeObject, AFFObject)
     VMETHOD(seek) = FileLikeObject_seek;
     VMETHOD(tell) = FileLikeObject_tell;
     VMETHOD(close) = FileLikeObject_close;
     VMETHOD(truncate) = FileLikeObject_truncate;

     VATTR(mode) = 'r';
END_VIRTUAL

static int FileBackedObject_truncate(FileLikeObject self, uint64_t offset) {
  FileBackedObject this=(FileBackedObject)self;

  ftruncate(this->fd, offset);
  return this->__super__->truncate(self, offset);
};

/** A file backed object extends FileLikeObject */
VIRTUAL(FileBackedObject, FileLikeObject)
     VMETHOD(Con) = FileBackedObject_Con;
     VMETHOD(super.super.Con) = FileBackedObject_AFFObject_Con;
     VMETHOD(super.super.finish) = FileBackedObject_finish;

     VMETHOD(super.read) = FileBackedObject_read;
     VMETHOD(super.write) = FileBackedObject_write;
     VMETHOD(super.close) = FileBackedObject_close;
     VMETHOD(super.truncate) = FileBackedObject_truncate;
END_VIRTUAL;

/** This is the constructor which will be used when we get
    instantiated as an AFFObject.
*/
static AFFObject ZipFile_AFFObject_Con(AFFObject self, char *urn, char mode) {
  ZipFile this = (ZipFile)self;

  self->mode = mode;

  if(urn) {
    // Ok, we need to create ourselves from a URN. We need a
    // FileLikeObject first. We ask the oracle what object should be
    // used as our underlying FileLikeObject:
    char *url = CALL(oracle, resolve, urn, NAMESPACE "stored");
    // We have no idea where we are stored:
    if(!url) {
      RaiseError(ERuntimeError, "Can not find the storage for Volume %s", urn);
      goto error;
    };

    self->urn = talloc_strdup(self, urn);
    // Call our other constructor to actually read this file:
    self = (AFFObject)this->Con((ZipFile)this, url, mode);
  } else {
    // Call ZipFile's AFFObject constructor.
    this->__super__->Con(self, urn, mode);
  };

  return self;
 error:
  talloc_free(self);
  return NULL;
};

static AFFObject ZipFile_finish(AFFObject self) {
  ZipFile this = (ZipFile)self;
  char *file_urn = CALL(oracle, resolve, self->urn, NAMESPACE "stored");

  // Make sure the constructor knows we want to create a new Zip
  // file if one doesnt already exist.
  self->mode = 'w';

  if(!file_urn) {
    RaiseError(ERuntimeError, "Volume %s has no " NAMESPACE "stored property?", self->urn);
    return NULL;
  };

#if 0
  // Do we need to truncate it?
  if(!CALL(oracle, resolve, self->urn, "aff2volatile:append")) {
    CALL(fd, truncate, 0);
  };

  CALL(oracle, set, 
       URNOF(self), 	       /* Source URI */
       AFF4_TYPE,            /* Attributte */
       AFF4_ZIP_VOLUME);              /* Value */

#endif
    
  return (AFFObject)CALL((ZipFile)this, Con, file_urn, 'w');
};


static ZipFile ZipFile_Con(ZipFile self, char *fd_urn, char mode) {
  char buffer[BUFF_SIZE+1];
  int length,i;
  FileLikeObject fd=NULL;

  memset(buffer,0,BUFF_SIZE+1);

  // Make sure we have a URN
  CALL((AFFObject)self, Con, NULL, mode);

  // Is there a file we need to read?
  fd = (FileLikeObject)CALL(oracle, open, self, fd_urn, mode);
  if(!fd) goto error;

  self->parent_urn = talloc_strdup(self, fd_urn);

  // Find the End of Central Directory Record - We read about 4k of
  // data and scan for the header from the end, just in case there is
  // an archive comment appended to the end
  CALL(fd, seek, -(int64_t)BUFF_SIZE, SEEK_END);
  length = CALL(fd, read, buffer, BUFF_SIZE);
  // Non fatal Error occured
  if(length<0) 
    goto exit;

  // Scan the buffer backwards for an End of Central Directory magic
  for(i=length; i>0; i--) {
    if(*(uint32_t *)(buffer+i) == 0x6054b50) {
      break;
    };
  };
  
  if(i==0) {
    // We could not find the CD - we assume its a new file and we
    // stick ourselves on the end of it.
    CALL(oracle, set, URNOF(self), AFF4_TYPE, AFF4_ZIP_VOLUME);
    CALL(oracle, set, URNOF(self), AFF4_DIRECTORY_OFFSET, from_int(fd->size));
  } else {
    self->end = (struct EndCentralDirectory *)talloc_memdup(self, buffer+i, 
							    sizeof(*self->end));
    int j=0;

    // Is there a comment field? We expect the comment field to be
    // exactly a URN. If it is we can update our notion of the URN to
    // be the same as that.
    if(self->end->comment_len == strlen(URNOF(self))) {
      char *comment = buffer + i + sizeof(*self->end);
      // Is it a fully qualified name?
      if(!memcmp(comment, ZSTRING_NO_NULL(FQN))) {
	// Update our URN from the comment.
	memcpy(URNOF(self),comment, strlen(URNOF(self)));
      };
    };

    // Make sure that the oracle knows about this volume:
    // FIXME: should be Volatile?
    // Note that our URN has changed above which means we can not set
    // any resolver properties until now that our URN is finalised.
    CALL(oracle, set, URNOF(self), AFF4_STORED, URNOF(fd));
    CALL(oracle, add, URNOF(fd), AFF4_CONTAINS, URNOF(self));
    CALL(oracle, set, URNOF(self), AFF4_TYPE, AFF4_ZIP_VOLUME);
    CALL(oracle, set, URNOF(self), AFF4_DIRECTORY_OFFSET, 
	 from_int(self->end->offset_of_cd));

    // Find the CD
    CALL(fd, seek, self->end->offset_of_cd, SEEK_SET);

    while(j<self->end->total_entries_in_cd_on_disk) {
      struct CDFileHeader cd_header;
      char *filename;
      char *escaped_filename;

      // The length of the struct up to the filename

      // Only read up to the filename member
      if(sizeof(cd_header) != 
	 CALL(fd, read, (char *)&cd_header, sizeof(cd_header)))
	goto error_reason;

      // Does the magic match?
      if(cd_header.magic != 0x2014b50)
	goto error_reason;

      // Now read the filename
      escaped_filename = talloc_array(self, char, cd_header.file_name_length+1);
						
      if(CALL(fd, read, escaped_filename, cd_header.file_name_length) != 
	 cd_header.file_name_length)
	goto error_reason;

      // Unescape the filename
      filename = unescape_filename(escaped_filename);
      // This is the fully qualified name
      filename = fully_qualified_name(filename, URNOF(self));

      // Tell the oracle about this new member
      CALL(oracle, set, filename, AFF4_STORED, URNOF(self));
      CALL(oracle, set, filename, AFF4_TYPE, AFF4_BLOB);
      // FIXME - This should be non volatile?
      CALL(oracle, add, URNOF(self), AFF4_CONTAINS, filename);

      CALL(oracle, set, filename, VOLATILE_NS "file_size", 
	   from_int(cd_header.file_size));

      CALL(oracle, set, filename, VOLATILE_NS "compression", 
	   from_int(cd_header.compression_method));

      CALL(oracle, set, filename, VOLATILE_NS "crc32", 
	   from_int(cd_header.crc32));

      CALL(oracle, set, filename, VOLATILE_NS "compress_size", 
	   from_int(cd_header.compress_size));
      
      CALL(oracle, set, filename, VOLATILE_NS "relative_offset_local_header", 
	   from_int(cd_header.relative_offset_local_header));

      // Skip the comments - we dont care about them
      CALL(fd, seek, cd_header.extra_field_len + 
	   cd_header.file_comment_length, SEEK_CUR);

      // Read the zip file itself
      {
	uint64_t current_offset = CALL(fd, tell);
	struct ZipFileHeader file_header;
	uint32_t file_offset;

	CALL(fd,seek, cd_header.relative_offset_local_header, SEEK_SET);
	CALL(fd, read, (char *)&file_header, sizeof(file_header));

	file_offset = cd_header.relative_offset_local_header +
	  sizeof(file_header) +
	  file_header.file_name_length + file_header.extra_field_len;

	CALL(oracle, set, filename, VOLATILE_NS "file_offset", 
	     from_int(file_offset));

	CALL(fd, seek, current_offset, SEEK_SET);
      };

      // Is this file a properties file?
      {
	int filename_length = strlen(filename);
	int properties_length = strlen("properties");
	int len;

	// We identify streams by their filename ending with "properties"
	// and parse out their properties:
	if(filename_length >= properties_length && 
	   !strcmp("properties", filename + filename_length - properties_length)) {
	  char *text = CALL(self, read_member, self, filename, &len);
	  char *context = dirname(filename);

	  //	  printf("Found property file %s\n%s", filename, text);

	  if(text) {
	    CALL(oracle, parse, context, text, len);
	    talloc_free(text);
	  };
	};
      };

      // Do we have as many CDFileHeaders as we expect?
      j++;
      talloc_free(escaped_filename);
    };   
  };

 exit:
  CALL(oracle, cache_return, (AFFObject)fd);
  return self;

 error_reason:
  RaiseError(EInvalidParameter, "%s is not a zip file", URNOF(fd));
 error:
  if(fd) {
    CALL(oracle, cache_return, (AFFObject)fd);
  };
  talloc_free(self);
  return NULL;
};

/*** NOTE - The buffer callers receive will not be owned by the
     callers. Callers should never free it nor steal it. The buffer is
     owned by the cache system and callers merely borrow a reference
     to it. length will be adjusted to the size of the buffer.
*/
static char *ZipFile_read_member(ZipFile self, void *ctx,
				 char *filename, 
				 int *length) {
  char *buffer;
  FileLikeObject fd;
  uint32_t file_size = parse_int(CALL(oracle, resolve, filename, 
				      VOLATILE_NS "file_size"));
  
  int compression_method = parse_int(CALL(oracle, resolve, filename, 
					  VOLATILE_NS "compression"));
  
  uint64_t file_offset = parse_int(CALL(oracle, resolve, filename,
					VOLATILE_NS "file_offset"));
  

  // This is the volume the filename is stored in
  char *volume_urn = CALL(oracle, resolve, filename, AFF4_STORED);

  // This is the file that backs this volume
  char *fd_urn = CALL(oracle, resolve, volume_urn, AFF4_STORED);
  
  fd = (FileLikeObject)CALL(oracle, open, self, fd_urn, 'r');
  if(!fd) return NULL;

  // We know how large we would like the buffer so we make it that big
  // (bit extra for null termination).
  buffer = talloc_size(ctx, file_size + 2);
  *length = file_size;

  // Go to the start of the file
  CALL(fd, seek, file_offset, SEEK_SET);

  if(compression_method == ZIP_DEFLATE) {
    int compressed_length = parse_int(CALL(oracle, resolve, filename, 
					   VOLATILE_NS "compress_size"));
    int uncompressed_length = *length;
    char *tmp = talloc_size(buffer, compressed_length);
    z_stream strm;

    memset(&strm, 0, sizeof(strm));

    //Now read the data in
    if(CALL(fd, read, tmp, compressed_length) != compressed_length)
      goto error;

    // Decompress it
    /** Set up our decompressor */
    strm.next_in = (unsigned char *)tmp;
    strm.avail_in = compressed_length;
    strm.next_out = (unsigned char *)buffer;
    strm.avail_out = uncompressed_length;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;

    if(inflateInit2(&strm, -15) != Z_OK) {
      RaiseError(ERuntimeError, "Failed to initialise zlib");
      goto error;
    };

    if(inflate(&strm, Z_FINISH) !=Z_STREAM_END || \
       strm.total_out != uncompressed_length) {
      RaiseError(ERuntimeError, "Failed to fully decompress chunk (%s)", strm.msg);
      goto error;
    };

    inflateEnd(&strm);
  } else if(compression_method == ZIP_STORED) {
    if(CALL(fd, read, buffer, *length) != *length) {
      RaiseError(EIOError, "Unable to read %d bytes from %s@%lld", *length, 
		 URNOF(fd), fd->readptr);
      goto error;
    };

  } else {
    RaiseError(EInvalidParameter, "Compression type %X unknown", compression_method);
    goto error;
  };

  // Here we have a good buffer - now calculate the crc32
  if(1){
    uLong crc = crc32(0L, Z_NULL, 0);
    uLong file_crc = parse_int(CALL(oracle, resolve, filename, VOLATILE_NS "crc32"));
    crc = crc32(crc, (unsigned char *)buffer,
		file_size);

    if(crc != file_crc) {
      RaiseError(EIOError, 
		 "CRC not matched on decompressed file %s", filename);
      goto error;
    };
  };

  CALL(oracle, cache_return, (AFFObject)fd);
  return buffer;
 error:
  CALL(oracle, cache_return, (AFFObject)fd);
  talloc_free(buffer);
  return NULL;
};

static FileLikeObject ZipFile_open_member(ZipFile self, char *filename, char mode,
				   char *extra, uint16_t extra_field_len,
				   int compression) {
  FileLikeObject result;

  switch(mode) {
  case 'w': {
    struct ZipFileHeader header;
    char *writer;
    FileLikeObject fd;
    // We start writing new files at this point
    uint64_t directory_offset = parse_int(CALL(oracle, resolve, 
					       URNOF(self), 
					       AFF4_DIRECTORY_OFFSET));
    char *escaped_filename;

    // Make sure the filename we write on the archive is relative, and
    // the filename we use with the resolver is fully qualified
    escaped_filename = escape_filename(relative_name(filename, URNOF(self)));
    filename = fully_qualified_name(filename, URNOF(self));

    // Check to see if this zip file is already open - a global lock
    // (Note if the resolver is external this will lock all other
    // writers). FIXME - this is a race - Implement atomic get/set
    // locking operation in the resolver.
    writer = CALL(oracle, resolve, URNOF(self),VOLATILE_NS "write_lock");
    if(writer) {
      RaiseError(ERuntimeError, "Unable to create a new member for writing '%s', when one is already writing '%s'", filename, writer);
      return NULL;
    };

    // Put a lock on the volume now:
    CALL(oracle, set, URNOF(self), VOLATILE_NS "write_lock", "1");

    // Indicate that the file is dirty - This means we will be writing
    // a new CD on it
    CALL(oracle, set, URNOF(self), VOLATILE_NS "dirty", "1");

    // Open our current volume for writing:
    fd = (FileLikeObject)CALL(oracle, open, self, self->parent_urn, 'w');
    if(!fd) return NULL;

    // Go to the start of the directory_offset
    CALL(fd, seek, directory_offset, SEEK_SET);

    // Write a file header on
    memset(&header, 0, sizeof(header));
    header.magic = 0x4034b50;
    header.version = 0x14;
    // We prefer to write trailing directory structures
    header.flags = 0x08;
    header.compression_method = compression;
    header.file_name_length = strlen(escaped_filename);
    header.extra_field_len = extra_field_len;
    
    CALL(fd, write,(char *)&header, sizeof(header));
    CALL(fd, write, ZSTRING_NO_NULL(escaped_filename));

    CALL(oracle, set, filename, VOLATILE_NS "compression", 
	 from_int(compression));
    CALL(oracle, set, filename, VOLATILE_NS "file_offset", 
	 from_int(fd->tell(fd)));
    CALL(oracle, set, filename, VOLATILE_NS "relative_offset_local_header", 
	 from_int(directory_offset));

    result = (FileLikeObject)CONSTRUCT(ZipFileStream, 
				       ZipFileStream, Con, self, 
				       filename, self->parent_urn, 
				       URNOF(self),
				       'w');
    CALL(oracle, cache_return, (AFFObject)fd);
    break;
  };
  case 'r': {
    if(compression != ZIP_STORED) {
      RaiseError(ERuntimeError, "Unable to open seekable member for compressed members.");
      break;
    };

    result = (FileLikeObject)CONSTRUCT(ZipFileStream, 
				       ZipFileStream, Con, self, 
				       filename, self->parent_urn,
				       URNOF(self),
				       'r');
    break;
  };
  default:
    RaiseError(ERuntimeError, "Unsupported mode '%c'", mode);
    return NULL;
  };

  return result;
};

static void ZipFile_close(ZipFile self) {
  // Dump the current CD. We expect our fd is seeked to the right
  // place:
  int k=0;
  char *_didModify = CALL(oracle, resolve, URNOF(self), VOLATILE_NS "dirty");

  // We iterate over all the items which are contained in the
  // volume. We then write them into the CD.
  if(_didModify) {
    struct EndCentralDirectory end;
    FileLikeObject fd = (FileLikeObject)CALL(oracle, open, self ,self->parent_urn, 'w');
    uint64_t directory_offset = parse_int(CALL(oracle, resolve, 
					       URNOF(self), 
					       AFF4_DIRECTORY_OFFSET));

    if(!fd) return;
    CALL(fd, seek, directory_offset, SEEK_SET);
    
    // Dump the central directory for this volume
    {
      Cache volume_dict = CALL(oracle->urn, get_item, URNOF(self));
      Cache i;
  
      // Now we search all the tuples with URNOF(self)
      // aff2volatile:contains *
      list_for_each_entry(i, &volume_dict->cache_list, cache_list) {
	char *attribute = (char *)i->key;
	// This is the fully qualified name of the member (the
	// resolver always has fully qualified names).
	char *value = (char *)i->data;
	
	// This is a zip member
	if(!strcmp(attribute, AFF4_CONTAINS)) {
	  struct CDFileHeader cd;
	  time_t epoch_time = parse_int(CALL(oracle, resolve, value, 
					     VOLATILE_NS "timestamp"));
	  struct tm *now = localtime(&epoch_time);
	  // This gets the relative name of the fqn
	  char *escaped_filename = escape_filename(relative_name(value, URNOF(self)));

	  LogWarnings("Writing archive member %s -> %s", escaped_filename, value);

	  memset(&cd, 0, sizeof(cd));

	  cd.magic = 0x2014b50;
	  cd.version_made_by = 0x317;
	  cd.version_needed = 0x14;
	  cd.compression_method = parse_int(CALL(oracle, resolve, value, 
						 VOLATILE_NS "compression"));
	  
	  cd.crc32 = parse_int(CALL(oracle, resolve, value, 
				     VOLATILE_NS "crc32"));

	  cd.dosdate = (now->tm_year + 1900 - 1980) << 9 | 
	    (now->tm_mon + 1) << 5 | now->tm_mday;
	  cd.dostime = now->tm_hour << 11 | now->tm_min << 5 | 
	    now->tm_sec / 2;

	  cd.file_name_length = strlen(escaped_filename);
	  cd.relative_offset_local_header = parse_int(CALL(oracle, resolve, value, 
				      VOLATILE_NS "relative_offset_local_header"));
	  cd.file_size = parse_int(CALL(oracle, resolve, value, 
					VOLATILE_NS "file_size"));
	  cd.compress_size = parse_int(CALL(oracle, resolve, value, 
					    VOLATILE_NS "compress_size"));
	  
	  CALL(fd, write, (char *)&cd, sizeof(cd));
	  CALL(fd, write, (char *)ZSTRING_NO_NULL(escaped_filename));
	  k++;
	};
      };
    };

    // Now write an end of central directory record
    memset(&end, 0, sizeof(end));
    end.magic = 0x6054b50;
    end.offset_of_cd = directory_offset;
    end.total_entries_in_cd_on_disk = k;
    end.total_entries_in_cd = k;
    end.size_of_cd = CALL(fd, tell) - directory_offset;
    end.comment_len = strlen(URNOF(self));

    // Make sure to add our URN to the comment field in the end
    CALL(fd, write, (char *)&end, sizeof(end));
    CALL(fd, write, ZSTRING_NO_NULL(URNOF(self)));

    CALL(oracle, cache_return, (AFFObject)fd);

    // Make sure the lock is removed from this volume now:
    CALL(oracle, del, URNOF(self), VOLATILE_NS "write_lock");

    // Close the fd
    CALL(fd, close);
  };
};

/** This is just a convenience function - real simple now */
static int ZipFile_writestr(ZipFile self, char *filename, 
		      char *data, int len, char *extra, int extra_len, 
		      int compression) {
  FileLikeObject fd = CALL(self, open_member, filename, 'w', extra, extra_len,
			   compression);
  if(fd) {
    len = CALL(fd, write, data, len);
    CALL(fd, close);
    return len;
  } else return -1;
};

VIRTUAL(ZipFile, AFFObject)
     VMETHOD(Con) = ZipFile_Con;
     VMETHOD(read_member) = ZipFile_read_member;
     VMETHOD(open_member) = ZipFile_open_member;
     VMETHOD(close) = ZipFile_close;
     VMETHOD(writestr) = ZipFile_writestr;
     VMETHOD(super.Con) = ZipFile_AFFObject_Con;
     VMETHOD(super.finish) = ZipFile_finish;

     VATTR(mode) = 'r';
END_VIRTUAL

/** container_urn is the URN of the ZipFile container which holds this
    member, parent_urn is the URN of the backing FileLikeObject which
    the zip file is written on. filename is the filename of this new
    zip member.
*/
static ZipFileStream ZipFileStream_Con(ZipFileStream self, char *filename, 
				       char *parent_urn, char *container_urn,
				       char mode) {
  char *fqn_filename = fully_qualified_name(filename, URNOF(self));

  self->mode = mode;
  self->container_urn = talloc_strdup(self, container_urn);
  self->compression = parse_int(CALL(oracle, resolve, 
				     fqn_filename, 
				     VOLATILE_NS "compression"));
  
  self->file_offset = parse_int(CALL(oracle, resolve, 
				     fqn_filename, 
				     VOLATILE_NS "file_offset"));
    
  URNOF(self) = talloc_strdup(self, filename);
  self->parent_urn = talloc_strdup(self, parent_urn);
  self->super.size = parse_int(CALL(oracle, resolve,
				    fqn_filename,
				    VOLATILE_NS "file_size"));

  if(mode=='w' && self->compression == ZIP_DEFLATE) {
    // Initialise the stream compressor
    memset(&self->strm, 0, sizeof(self->strm));
    self->strm.next_in = talloc_size(self, BUFF_SIZE);
    self->strm.next_out = talloc_size(self, BUFF_SIZE);

    if(deflateInit2(&self->strm, 9, Z_DEFLATED, -15,
		    9, Z_DEFAULT_STRATEGY) != Z_OK) {
      RaiseError(ERuntimeError, "Unable to initialise zlib (%s)", self->strm.msg);
      goto error;
    };
  };


  // Set important parameters in the zinfo
  return self;

 error:
  talloc_free(self);
  return NULL;
};

/**
   This zlib trickery comes from http://www.zlib.net/zlib_how.html
**/
static int ZipFileStream_write(FileLikeObject self, char *buffer, unsigned long int length) {
  ZipFileStream this = (ZipFileStream)self;
  int result=0;
  FileLikeObject fd = (FileLikeObject)CALL(oracle, open, self, this->parent_urn, 'w');

  // Update the crc:
  this->crc32 = crc32(this->crc32, 
		      (unsigned char*)buffer,
		      length);

  /** Position our write pointer */
  CALL(fd, seek, this->file_offset + self->readptr, SEEK_SET);

  // Is this compressed?
  if(this->compression == ZIP_DEFLATE) {
    unsigned char compressed[BUFF_SIZE];

    /** Give the buffer to zlib */
    this->strm.next_in = (unsigned char *)buffer;
    this->strm.avail_in = length;

    /** We spin here until zlib consumed all the data */
    do {
      int ret;

      this->strm.avail_out = BUFF_SIZE;
      this->strm.next_out = compressed;
      
      ret = deflate(&this->strm, Z_NO_FLUSH);
      ret = CALL(fd, write, (char *)compressed, 
		 BUFF_SIZE - this->strm.avail_out);
      if(ret<0) return ret;
      result += ret;
    } while(this->strm.avail_out == 0);

  } else {
    /** Without compression, we just write the buffer right away */
    result = CALL(fd, write, buffer, length);
    if(result<0) 
      goto exit;
  }; 

  /** Update our compressed size here */
  this->compress_size += result;

  /** The readptr and the size are advanced by the uncompressed amount
   */
  self->readptr += length;
  self->size = max(self->size, self->readptr);
  
  exit:
  CALL(oracle, cache_return, (AFFObject)fd);
  return result;
};

static int ZipFileStream_read(FileLikeObject self, char *buffer,
			      unsigned long int length) {
  ZipFileStream this = (ZipFileStream)self;
  int len;
  FileLikeObject fd = (FileLikeObject)CALL(oracle, open, self, this->parent_urn, 'r');

  /** Position our write pointer */
  CALL(fd, seek, this->file_offset + self->readptr, SEEK_SET);
  len = CALL(fd, read, buffer, length);
  self->readptr += len;

  CALL(oracle, cache_return, (AFFObject)fd);
  return len;
};

static void ZipFileStream_close(FileLikeObject self) {
  ZipFileStream this = (ZipFileStream)self;
  FileLikeObject fd;
  
  if(this->mode == 'r') return;
  
  fd = (FileLikeObject)CALL(oracle, open, self, this->parent_urn, 'w');
  if(!fd) {
    RaiseError(ERuntimeError, "Unable to open file %s", this->parent_urn);
    return;
  };

  if(this->compression == ZIP_DEFLATE) {
    unsigned char compressed[BUFF_SIZE];

    do {
      int ret;

      this->strm.avail_out = BUFF_SIZE;
      this->strm.next_out = compressed;
      
      ret = deflate(&this->strm, Z_FINISH);
      CALL(fd, seek, this->file_offset + self->readptr, SEEK_SET);
      ret = CALL(fd, write, (char *)compressed, 
		 BUFF_SIZE - this->strm.avail_out);
      self->readptr += ret;

      this->compress_size += ret;
    } while(this->strm.avail_out == 0);
    
    (void)deflateEnd(&this->strm);
 };

  // Store important information about this file
  CALL(oracle, add, this->container_urn, AFF4_CONTAINS, URNOF(self));
  CALL(oracle, set, URNOF(self), VOLATILE_NS "stored", this->container_urn);
  CALL(oracle, set, URNOF(self), VOLATILE_NS "timestamp", from_int(time(NULL)));
  CALL(oracle, set, URNOF(self), VOLATILE_NS "file_size", from_int(self->size));
  CALL(oracle, set, URNOF(self), VOLATILE_NS "compress_size", from_int(this->compress_size));
  CALL(oracle, set, URNOF(self), VOLATILE_NS "crc32", from_int(this->crc32));

  // Put the description header on
  CALL(fd, seek, this->file_offset + self->readptr, SEEK_SET);
  CALL(fd, write, (char *)&this->crc32,
       sizeof(this->crc32));
  CALL(fd, write, (char *)&this->compress_size,
       sizeof(uint32_t));
  CALL(fd, write, (char *)&self->size,
       sizeof(uint32_t));

  // This is the point where we will be writing the next file.
  CALL(oracle, set, this->container_urn, AFF4_DIRECTORY_OFFSET,
       from_int(fd->tell(fd)));

  // Make sure the lock is removed from this volume now:
  CALL(oracle, del, this->container_urn, VOLATILE_NS "write_lock");
  CALL(oracle, cache_return, (AFFObject)fd);
  talloc_free(self);
};

VIRTUAL(ZipFileStream, FileLikeObject)
     VMETHOD(Con) = ZipFileStream_Con;
     VMETHOD(super.write) = ZipFileStream_write;
     VMETHOD(super.read) = ZipFileStream_read;
     VMETHOD(super.close) = ZipFileStream_close;
END_VIRTUAL

void print_cache(Cache self) {
  Cache i;

  list_for_each_entry(i, &self->cache_list, cache_list) {
    printf("%s %p %s\n",(char *) i->key,i->data, (char *)i->data);
  };
};

char *fully_qualified_name(char *filename, char *volume_urn) {
  static char fqn_filename[BUFF_SIZE];

  // We are always called with a fully qualified filename. 
  if(strstr(filename, FQN) != filename) {
    snprintf(fqn_filename, BUFF_SIZE, "%s/%s", volume_urn, filename);
    return fqn_filename;
  } else {
    return filename;
  };
};

char *relative_name(char *name, char *volume_urn) {
  if(strstr(name, volume_urn) == name) {
    return name + strlen(volume_urn)+1;
  };

  return name;
};
