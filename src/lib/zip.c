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

/** Implementation of Caches */

/** A destructor on the cache object to automatically unlink up from
    the lists.
*/
static int Cache_destructor(void *this) {
  Cache self = (Cache) this;
  list_del(&self->cache_list);
  list_del(&self->hash_list);

  // We can automatically maintain a tally of elements in the cache
  // because we have a reference to the main cache object here.
  self->cache_head->cache_size--;
  return 0;
};

/** A max_cache_size of 0 means we never expire anything */
static Cache Cache_Con(Cache self, int hash_table_width, int max_cache_size) {
  self->hash_table_width = hash_table_width;
  self->max_cache_size = max_cache_size;

  INIT_LIST_HEAD(&self->cache_list);
  INIT_LIST_HEAD(&self->hash_list);

  // Install our destructor
  talloc_set_destructor((void *)self, Cache_destructor);
  return self;
};

/** Quick and simple */
static unsigned int Cache_hash(Cache self, void *key) {
  char *name = (char *)key;
  int len = strlen(name);
  char result = 0;
  int i;
  for(i=0; i<len; i++) 
    result ^= name[i];

  return result % self->hash_table_width;
};

static int Cache_cmp(Cache self, void *other) {
  return strcmp((char *)self->key, (char *)other);
};

static Cache Cache_put(Cache self, void *key, void *data, int data_len) {
  unsigned int hash;
  Cache hash_list_head;
  Cache new_cache;
  Cache i,j;

  // Check to see if we need to expire something else. We do this
  // first to avoid the possibility that we might expire the same key
  // we are about to add.
  while(self->max_cache_size > 0 && self->cache_size >= self->max_cache_size) {
    list_for_each_entry(i, &self->cache_list, cache_list) {
      talloc_free(i);
      break;
    };
  };

  if(!self->hash_table)
    self->hash_table = talloc_array(self, Cache, self->hash_table_width);

  hash = CALL(self, hash, key);
  hash_list_head = self->hash_table[hash];
  new_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);
  // Make sure the new cache member knows where the list head is. We
  // only keep stats about the cache here.
  new_cache->cache_head = self;

  // Take over the data
  new_cache->key = key;
  new_cache->data = data;
  new_cache->data_len = data_len;
  talloc_steal(new_cache, key);
  talloc_steal(new_cache, data);

  if(!hash_list_head) {
    hash_list_head = self->hash_table[hash] = CONSTRUCT(Cache, Cache, Con, self, 
							HASH_TABLE_SIZE, -10);
    talloc_set_name_const(hash_list_head, "Hash Head");
  };

  //  printf("Adding %p\n", new_cache);
  list_add_tail(&new_cache->hash_list, &self->hash_table[hash]->hash_list);
  list_add_tail(&new_cache->cache_list, &self->cache_list);
  self->cache_size ++;
 
  return new_cache;
};

static Cache Cache_get(Cache self, void *key) {
  int hash;
  Cache hash_list_head;
  Cache i,j;

  hash = CALL(self, hash, key);
  if(!self->hash_table) return NULL;

  hash_list_head = self->hash_table[hash];

  // There are 2 lists each Cache object is on - the hash list is a
  // shorter list at the end of each hash table slot, while the cache
  // list is a big list of all objects in the cache. We find the
  // object using the hash list, but expire the object based on the
  // cache list which includes all objects in the case.
  if(!hash_list_head) return NULL;
  list_for_each_entry(i, &hash_list_head->hash_list, hash_list) {
    if(!CALL(i, cmp, key)) {
      // Thats it - we remove it from where its in and put it on the
      // tail:
      list_move(&i->cache_list, &self->cache_list);
      list_move(&i->hash_list, &hash_list_head->hash_list);
      //printf("Getting %p\n", i);
      return i;
    };
  };
  
  return NULL;
};

void *Cache_get_item(Cache self, char *key) {
  Cache tmp = CALL(self, get, key);

  if(tmp && tmp->data)
    return tmp->data;

  return NULL;
};

VIRTUAL(Cache, Object)
     VMETHOD(Con) = Cache_Con;
     VMETHOD(put) = Cache_put;
     VMETHOD(cmp) = Cache_cmp;
     VMETHOD(hash) = Cache_hash;
     VMETHOD(get) = Cache_get;
     VMETHOD(get_item) = Cache_get_item;
END_VIRTUAL

/** Implementation of FileBackedObject.

FileBackedObject is a FileLikeObject which uses a read file to back
itself.
*/
static int close_fd(void *self) {
  FileBackedObject this = (FileBackedObject)self;
  close(this->fd);
  return 0;
};

static FileBackedObject FileBackedObject_Con(FileBackedObject self, 
				      char *filename, char mode) {
  int flags;

  switch(mode) {
  case 'w':
    flags =  O_CREAT|O_RDWR|O_TRUNC|O_BINARY;
    break;
  case 'a':
    flags =  O_CREAT|O_RDWR|O_APPEND|O_BINARY;
    break;
  case 'r':
    flags = O_BINARY | O_RDONLY;
    break;
  default:
    RaiseError(EInvalidParameter, "Unknown mode '%c'", mode);
    return NULL;
  };
  self->fd = open(filename, flags, S_IRWXU | S_IRWXG | S_IRWXO);
  if(self->fd<0){
    RaiseError(EIOError, "Can't open %s (%s)", filename, strerror(errno));
    return NULL;
  };

  // Make sure that we close the file when we free ourselves - we dont
  // want to leak file handles
  talloc_set_destructor((void *)self, close_fd);

  // Work out what the file size is:
  self->super.size = lseek(self->fd, 0, SEEK_END);
  self->super.name = talloc_strdup(self, filename);

  // Set our uri
  self->super.super.urn = talloc_asprintf(self, "file://%s", filename);

  return self;
};

// This is the low level constructor for FileBackedObject.
static AFFObject FileBackedObject_AFFObject_Con(AFFObject self, char *urn) {
  FileBackedObject this = (FileBackedObject)self;

  if(urn) {
    // If the urn starts with file:// we open the filename, otherwise
    // we try to open the actual file itself:
    if(!memcmp(urn, ZSTRING_NO_NULL("file://"))) {
      this->Con(this, urn + strlen("file://"), 'r');
    } else {
      this->Con(this, urn, 'r');
    };
  } else {
    this->__super__->super.Con((AFFObject)this, urn);
  };

  return self;
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
    RaiseError(EIOError, "Unable to read from %s (%s)", self->name, strerror(errno));
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
    RaiseError(EIOError, "Unable to write to %s (%s)", self->name, strerror(errno));
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
END_VIRTUAL

/** A file backed object extends FileLikeObject */
VIRTUAL(FileBackedObject, FileLikeObject)
     VMETHOD(Con) = FileBackedObject_Con;
     VMETHOD(super.super.Con) = FileBackedObject_AFFObject_Con;
     VMETHOD(super.super.finish) = FileBackedObject_finish;

     VMETHOD(super.read) = FileBackedObject_read;
     VMETHOD(super.write) = FileBackedObject_write;
     VMETHOD(super.close) = FileBackedObject_close;
END_VIRTUAL;

/** Now implement Zip file support */
static ZipInfo ZipInfo_Con(ZipInfo self, char *fd_urn) {
  self->fd_urn = talloc_strdup(self, fd_urn);

  return self;
};

VIRTUAL(ZipInfo, Object)
     VMETHOD(Con) = ZipInfo_Con;
END_VIRTUAL

static void ZipFile_add_zipinfo_to_cache(ZipFile self, ZipInfo zip) {
  // The cache steals the filename
  CALL(self->zipinfo_cache, put, 
       talloc_strdup(self, zip->filename), zip, sizeof(*zip));
};

static ZipFile ZipFile_Con(ZipFile self, char *fd_urn) {
  char buffer[4096];
  int length,i;
  FileLikeObject fd = (FileLikeObject)CALL(oracle, open, self, fd_urn);

  // A max_cache_size of 1e6 ensure that we do not expire any ZipInfo
  // objects.
  self->zipinfo_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 1E06);
  self->_didModify = 0;

  // If we were not given something to read, we are done.
  if(!fd) return self;

  self->parent_urn = talloc_strdup(self, fd_urn);

  // Find the End of Central Directory Record - We read about 4k of
  // data and scan for the header from the end, just in case there is
  // an archive comment appended to the end
  CALL(fd, seek, -(int64_t)sizeof(buffer), SEEK_END);
  length = CALL(fd, read, buffer, sizeof(buffer));
  // Error occured
  if(length<0) 
    goto error;

  // Scan the buffer backwards for an End of Central Directory magic
  for(i=length; i>0; i--) {
    if(*(uint32_t *)(buffer+i) == 0x6054b50) {
      break;
    };
  };
  
  if(i==0) {
    goto error_reason;
  } else {
    self->end = (struct EndCentralDirectory *)talloc_memdup(self, buffer+i, 
							    sizeof(*self->end));
    int i=0;

    // Find the CD
    CALL(fd, seek, self->end->offset_of_cd, SEEK_SET);

    while(i<self->end->total_entries_in_cd_on_disk) {
      ZipInfo zip = CONSTRUCT(ZipInfo, ZipInfo, Con, self, fd_urn);
      // The length of the struct up to the filename

      // Only read up to the filename member
      if(sizeof(struct CDFileHeader) != 
	 CALL(fd, read, (char *)&zip->cd_header, sizeof(struct CDFileHeader)))
	goto error_reason;

      // Does the magic match?
      if(zip->cd_header.magic != 0x2014b50)
	goto error_reason;

      // Now read the filename
      zip->filename = talloc_array(zip, char, zip->cd_header.file_name_length+1);
						
      if(CALL(fd, read, zip->filename,
	      zip->cd_header.file_name_length) == 0)
	goto error_reason;

      CALL(self, add_zipinfo_to_cache, zip);

      // Skip the comments - we dont care about them
      CALL(fd, seek, zip->cd_header.extra_field_len + 
	   zip->cd_header.file_comment_length, SEEK_CUR);

      // Read the zip file itself
      {
	uint64_t current_offset = CALL(fd, tell);
	struct ZipFileHeader header;

	CALL(fd,seek, zip->cd_header.relative_offset_local_header, SEEK_SET);
	CALL(fd, read, (char *)&header, sizeof(header));

	zip->file_offset = zip->cd_header.relative_offset_local_header +
	  sizeof(struct ZipFileHeader) +
	  header.file_name_length + header.extra_field_len;

	CALL(fd, seek, current_offset, SEEK_SET);
      };
      // Do we have as many CDFileHeaders as we expect?
      i++;
    };   
  };

  return self;

 error_reason:
    RaiseError(EInvalidParameter, "%s is not a zip file", fd->name);
 error:
    talloc_free(self);
    return NULL;
};

static ZipInfo ZipFile_fetch_ZipInfo(ZipFile self, char *filename) {
  Cache result = CALL(self->zipinfo_cache, get, (void *)filename);
  if(!result) {
    return NULL;
  };

  return (ZipInfo)result->data;
};

/*** NOTE - The buffer callers receive will not be owned by the
     callers. Callers should never free it nor steal it. The buffer is
     owned by the cache system and callers merely borrow a reference
     to it. length will be adjusted to the size of the buffer.
*/
static char *ZipFile_read_member(ZipFile self, void *ctx,
				 char *filename, 
				 int *length) {
  ZipInfo zip=NULL;
  char compression_method;
  struct ZipFileHeader header;
  int read_length = sizeof(header);
  char *buffer;
  FileLikeObject fd;
  
  zip = CALL(self, fetch_ZipInfo, filename);
  if(!zip) return NULL;

  fd = (FileLikeObject)CALL(oracle, open, self, zip->fd_urn);
  if(!fd) return NULL;

  // We know how large we would like the buffer so we make it that big
  // (bit extra for null termination).
  buffer = talloc_size(ctx, zip->cd_header.file_size + 2);
  *length = zip->cd_header.file_size;

  compression_method = zip->cd_header.compression_method;

  // Read the File Header
  CALL(fd, seek, zip->cd_header.relative_offset_local_header,
       SEEK_SET);
  if(CALL(fd, read,(char *)&header, read_length) != read_length)
    goto error;
  
  // Check the magic
  if(header.magic != 0x4034b50) {
    RaiseError(ERuntimeError, "Unable to find file header");
    goto error;
  };

  // Skip the filename
  CALL(fd, seek, header.file_name_length + 
       header.extra_field_len, SEEK_CUR);
  
  if(compression_method == ZIP_DEFLATE) {
    int compressed_length = zip->cd_header.compress_size;
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
		 fd->name, fd->readptr);
      goto error;
    };

  } else {
    RaiseError(EInvalidParameter, "Compression type %X unknown", compression_method);
    goto error;
  };

  // Here we have a good buffer - now calculate the crc32
  if(1){
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (unsigned char *)buffer,
		zip->cd_header.file_size);

    if(crc != zip->cd_header.crc32) {
      RaiseError(EIOError, 
		 "CRC not matched on decompressed file %s", zip->filename);
      goto error;
    };
  };

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
    char *writer =CALL(oracle, resolve, ((AFFObject)self)->urn,
		       "aff2volatile:write_lock");
    FileLikeObject fd;
    
    // Check to see if this zip file is already open - a global lock
    // (Note if the resolver is external this will lock all other
    // writers).
    if(writer) {
      RaiseError(ERuntimeError, "Unable to create a new member for writing '%s', when one is already writing '%s'", filename, writer);
      return NULL;
    };

    // Open our current volume:
    fd = (FileLikeObject)CALL(oracle, open, self, self->parent_urn);

    // FIXME - directory_offset should be stored in volatile storage
    CALL(fd, seek, self->directory_offset, SEEK_SET);

    // Write a file header on
    memset(&header, 0, sizeof(header));
    header.magic = 0x4034b50;
    header.version = 0x14;
    // We prefer to write trailing directory structures
    header.flags = 0x08;
    header.compression_method = compression;
    header.file_name_length = strlen(filename);
    header.extra_field_len = extra_field_len;
    
    CALL(fd, write,(char *)&header, sizeof(header));
    CALL(fd, write, ZSTRING_NO_NULL(filename));
    //    CALL(fd, write, ZSTRING_NO_NULL("AF"));
    //    CALL(fd, write, (char *)&extra_field_len, sizeof(extra_field_len));
    //    CALL(fd, write, extra, extra_field_len);

    result = (FileLikeObject)CONSTRUCT(ZipFileStream, 
				       ZipFileStream, Con, self, self,
				       self->parent_urn, filename, 'w', compression,
				       CALL(fd, tell),
				       self->directory_offset);

    CALL(oracle, cache_return, (AFFObject)fd);
    self->_didModify = 1;
    break;
  };
  case 'r': {
    ZipInfo zip;

    if(compression != ZIP_STORED) {
      RaiseError(ERuntimeError, "Unable to open seekable member for compressed members.");
      break;
    };

    zip = CALL(self, fetch_ZipInfo, filename);

    result = (FileLikeObject)CONSTRUCT(ZipFileStream, 
				       ZipFileStream, Con, self, self,
				       self->parent_urn, filename, 'r', compression,
				       zip->file_offset,
				       self->directory_offset);
    result->size = zip->cd_header.file_size;
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
  Cache i;
  int k=0;

  // We iterate over all the ZipInfo hashes we have and see which ones
  // point at this file. We will need to be rewritten back to this
  // file:
  if(self->_didModify) {
    struct EndCentralDirectory end;
    FileLikeObject fd = (FileLikeObject)CALL(oracle, open, self ,self->parent_urn);

    CALL(fd, seek, self->directory_offset, SEEK_SET);
    
    // FIXME - this should be done through the resolver
    list_for_each_entry(i, &self->zipinfo_cache->cache_list, cache_list) {
      ZipInfo j = (ZipInfo)i->data;

      // Its stored in this volume
      if(j->fd_urn == self->parent_urn) {
	// Write the CD file entry
	CALL(fd, write, (char *)&j->cd_header, sizeof(j->cd_header));
	CALL(fd, write, ZSTRING_NO_NULL(j->filename));
	k++;
      };
    };

    // Now write an end of central directory record
    memset(&end, 0, sizeof(end));
    end.magic = 0x6054b50;
    end.offset_of_cd = self->directory_offset;
    end.total_entries_in_cd_on_disk = k;
    end.total_entries_in_cd = k;
    end.size_of_cd = CALL(fd, tell) - self->directory_offset;
    
    CALL(fd, write, (char *)&end, sizeof(end));
    CALL(oracle, cache_return, (AFFObject)fd);

    // Make sure the lock is removed from this volume now:
    CALL(oracle, del, ((AFFObject)self)->urn, "aff2volatile:write_lock");
  };
};

/** This is just a convenience function - real simple now */
static void ZipFile_writestr(ZipFile self, char *filename, 
		      char *data, int len, char *extra, int extra_len, 
		      int compression) {
  FileLikeObject fd = CALL(self, open_member, filename, 'w', extra, extra_len,
			   compression);
  if(fd) {
    CALL(fd, write, data, len);
    CALL(fd, close);
  };
};

VIRTUAL(ZipFile, AFFObject)
     VMETHOD(Con) = ZipFile_Con;
     VMETHOD(read_member) = ZipFile_read_member;
     VMETHOD(open_member) = ZipFile_open_member;
     VMETHOD(close) = ZipFile_close;
     VMETHOD(writestr) = ZipFile_writestr;
     VMETHOD(fetch_ZipInfo) = ZipFile_fetch_ZipInfo;
     VMETHOD(add_zipinfo_to_cache) = ZipFile_add_zipinfo_to_cache;
END_VIRTUAL


static ZipFileStream ZipFileStream_Con(ZipFileStream self, ZipFile zip, 
				       char *fd_urn, char *filename,
				       char mode, int compression, 
				       uint64_t zip_offset,
				       uint64_t header_offset) {
  self->zip = zip;
  self->mode = mode;
  self->super.super.urn = filename;
  self->parent_urn = talloc_strdup(self, fd_urn);

  if(mode=='w' && compression == ZIP_DEFLATE) {
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

  self->zip_offset = zip_offset;
  self->zinfo = CONSTRUCT(ZipInfo, ZipInfo, Con, self, fd_urn);
  
  // Set important parameters in the zinfo
  {
    struct CDFileHeader *cd = &self->zinfo->cd_header;
    time_t epoch_time = time(NULL);
    struct tm *now = localtime(&epoch_time);
    
    cd->magic = 0x2014b50;
    cd->version_made_by = 0x317;
    cd->version_needed = 0x14;
    cd->compression_method = compression;
    cd->crc32 = crc32(0L, Z_NULL, 0);
    cd->dosdate = (now->tm_year + 1900 - 1980) << 9 | (now->tm_mon + 1) << 5 | now->tm_mday;
    cd->dostime = now->tm_hour << 11 | now->tm_min << 5 | now->tm_sec / 2;
    cd->file_name_length = strlen(filename);
    self->zinfo->filename = talloc_strdup(self->zinfo, filename);
    cd->relative_offset_local_header = header_offset;
    cd->file_size = 0;
    cd->compress_size = 0;
  };

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
  FileLikeObject fd = (FileLikeObject)CALL(oracle, open, self, this->parent_urn);

  // Update the crc:
  this->zinfo->cd_header.crc32 = crc32(this->zinfo->cd_header.crc32, 
				       (unsigned char*)buffer,
				       length);

  /** Position our write pointer */
  CALL(fd, seek, this->zip_offset + self->readptr, SEEK_SET);
  if(this->zinfo->cd_header.compression_method == ZIP_DEFLATE) {
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
    if(result<0) return result;
  }; 

  /** Update our compressed size here */
  this->zinfo->cd_header.compress_size += result;

  /** The readptr and the size are advanced by the uncompressed amount
   */
  self->readptr += length;
  self->size = max(self->size, self->readptr);
  
  return result;
};

static int ZipFileStream_read(FileLikeObject self, char *buffer,
			      unsigned long int length) {
  ZipFileStream this = (ZipFileStream)self;
  int len;
  FileLikeObject fd = (FileLikeObject)CALL(oracle, open, self, this->parent_urn);

  /** Position our write pointer */
  CALL(fd, seek, this->zip_offset + self->readptr, SEEK_SET);
  len = CALL(fd, read, buffer, length);
  self->readptr += len;

  CALL(oracle, cache_return, (AFFObject)fd);
  return len;
};

static void ZipFileStream_close(FileLikeObject self) {
  ZipFileStream this = (ZipFileStream)self;
  FileLikeObject fd;
  
  if(this->mode == 'r') return;
  
  fd = (FileLikeObject)CALL(oracle, open, self, this->parent_urn);
  if(!fd) {
    
  };
  if(this->zinfo->cd_header.compression_method == ZIP_DEFLATE) {
    unsigned char compressed[BUFF_SIZE];

    do {
      int ret;

      this->strm.avail_out = BUFF_SIZE;
      this->strm.next_out = compressed;
      
      ret = deflate(&this->strm, Z_FINISH);
      CALL(fd, seek, this->zip_offset + self->readptr, SEEK_SET);
      ret = CALL(fd, write, (char *)compressed, 
		 BUFF_SIZE - this->strm.avail_out);
      self->readptr += ret;

      this->zinfo->cd_header.compress_size += ret;
    } while(this->strm.avail_out == 0);
    
    (void)deflateEnd(&this->strm);
 };

  // Update the length in zinfo:
  this->zinfo->cd_header.file_size = self->size;

  CALL(fd, seek, this->zip_offset + self->readptr, SEEK_SET);
  CALL(fd, write, (char *)&this->zinfo->cd_header.crc32,
       sizeof(this->zinfo->cd_header.crc32));
  CALL(fd, write, (char *)&this->zinfo->cd_header.compress_size,
       sizeof(uint32_t));
  CALL(fd, write, (char *)&this->zinfo->cd_header.file_size,
       sizeof(uint32_t));

  // Now the zinfo is complete we can add it to the volume cache:
  CALL(this->zip->zipinfo_cache, put, 
       this->zinfo->filename, this->zinfo, sizeof(*this->zinfo));

  // Make this is the point where we will be writing the next file.
  this->zip->directory_offset = CALL(fd, tell);
  
  // Make sure the lock is removed from this volume now:
  CALL(oracle, del, this->parent_urn, "aff2volatile:write_lock");

  // Free ourselves
  //  talloc_free(self);
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
