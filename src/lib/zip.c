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
  list_for_each_entry_safe(i, j, &self->cache_list, cache_list) {
    if(self->max_cache_size==0 || self->cache_size < self->max_cache_size) 
      break;
    else {
      self->cache_size--;
      // By freeing the cache element it will take care of removing
      // itself from whatever lists it needs to using its destructor.
      talloc_free(i);
    };
  };

  if(!self->hash_table)
    self->hash_table = talloc_array(self, Cache, self->hash_table_width);

  hash = CALL(self, hash, key);
  hash_list_head = self->hash_table[hash];
  new_cache = CONSTRUCT(Cache, Cache, Con, self->hash_table, HASH_TABLE_SIZE, 0);

  // Take over the data
  new_cache->key = key;
  new_cache->data = data;
  new_cache->data_len = data_len;
  talloc_steal(new_cache, key);
  talloc_steal(new_cache, data);

  if(!hash_list_head) {
    hash_list_head = self->hash_table[hash] = CONSTRUCT(Cache, Cache, Con, self, 
							HASH_TABLE_SIZE, 0);
    talloc_set_name_const(hash_list_head, "Hash Head");
  };

  list_add_tail(&new_cache->hash_list, &hash_list_head->hash_list);
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
  list_for_each_entry_safe(i, j, &hash_list_head->hash_list, hash_list) {
    if(!CALL(i, cmp, key)) {
      // Thats it - we remove it from where it in and put it on the
      // tail:
      list_del(&i->cache_list);
      list_add_tail(&i->cache_list, &self->cache_list);
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
static ZipInfo ZipInfo_Con(ZipInfo self, FileLikeObject fd) {
  self->fd = fd;

  return self;
};

VIRTUAL(ZipInfo, Object)
     VMETHOD(Con) = ZipInfo_Con;
END_VIRTUAL

static void ZipFile_add_zipinfo_to_cache(ZipFile self, ZipInfo zip) {
  CALL(self->zipinfo_cache, put, zip->filename, zip, sizeof(*zip));
};

static ZipFile ZipFile_Con(ZipFile self, FileLikeObject fd) {
  char buffer[4096];
  int length,i;
  self->fd = fd;

  // A max_cache_size of 1e6 ensure that we do not expire any ZipInfo
  // objects.
  self->zipinfo_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 1E06);

  // This cache is used to maintain entire zip archive members
  self->file_data_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, CACHE_SIZE);
  
  // And this is the cache for the extra field
  self->extra_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, CACHE_SIZE);

  self->_didModify = 0;

  // If we were not given something to read, we are done.
  if(!fd) return self;

  // Find the End of Central Directory Record - We read about 4k of
  // data and scan for the header from the end, just in case there is
  // an archive comment appended to the end
  CALL(self->fd, seek, -(int64_t)sizeof(buffer), SEEK_END);
  length = CALL(self->fd, read, buffer, sizeof(buffer));
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
    CALL(self->fd, seek, self->end->offset_of_cd, SEEK_SET);

    while(i<self->end->total_entries_in_cd_on_disk) {
      ZipInfo zip = CONSTRUCT(ZipInfo, ZipInfo, Con, self, self->fd);
      // The length of the struct up to the filename

      // Only read up to the filename member
      if(sizeof(struct CDFileHeader) != 
	 CALL(self->fd, read, (char *)&zip->cd_header, sizeof(struct CDFileHeader)))
	goto error_reason;

      // Does the magic match?
      if(zip->cd_header.magic != 0x2014b50)
	goto error_reason;

      // Now read the filename
      zip->filename = talloc_array(zip, char, zip->cd_header.file_name_length+1);
						
      if(CALL(self->fd, read, zip->filename,
	      zip->cd_header.file_name_length) == 0)
	goto error_reason;

      CALL(self, add_zipinfo_to_cache, zip);

      // Skip the comments - we dont care about them
      CALL(self->fd, seek, zip->cd_header.extra_field_len + 
	   zip->cd_header.file_comment_length, SEEK_CUR);

      // Do we have as many CDFileHeaders as we expect?
      i++;
    };   
  };

  return self;

 error_reason:
    RaiseError(EInvalidParameter, "%s is not a zip file", self->fd->name);
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
static char *ZipFile_read_member(ZipFile self, char *filename, 
			  int *length) {
  ZipInfo zip=NULL;
  char compression_method;
  struct ZipFileHeader header;
  int read_length = sizeof(header);
  Cache cached_data;
  char *buffer;
  uint64_t original_offset;
  
  zip = CALL(self, fetch_ZipInfo, filename);
  original_offset = CALL(self->fd, tell);
  if(!zip) return NULL;

  // Does this ZipInfo have a cache? If so just return that
  cached_data = CALL(self->file_data_cache, get, filename); 
  if(cached_data) {
    *length = cached_data->data_len;
    buffer = cached_data->data;

    return buffer;
  };

  // Ok we have to read it the long way:

  // We know how large we would like the buffer so we make it that big
  // (bit extra for null termination).
  buffer = talloc_size(self, zip->cd_header.file_size + 2);
  *length = zip->cd_header.file_size;

  compression_method = zip->cd_header.compression_method;

  // Read the File Header
  CALL(self->fd, seek, zip->cd_header.relative_offset_local_header,
       SEEK_SET);
  if(CALL(self->fd, read,(char *)&header, read_length) != read_length)
    goto error;
  
  // Check the magic
  if(header.magic != 0x4034b50) {
    RaiseError(ERuntimeError, "Unable to find file header");
    goto error;
  };

  // Skip the filename
  CALL(self->fd, seek, header.file_name_length + 
       header.extra_field_len, SEEK_CUR);
  
  if(compression_method == ZIP_DEFLATE) {
    int compressed_length = zip->cd_header.compress_size;
    int uncompressed_length = *length;
    char *tmp = talloc_size(buffer, compressed_length);
    z_stream strm;

    memset(&strm, 0, sizeof(strm));

    //Now read the data in
    if(CALL(self->fd, read, tmp, compressed_length) != compressed_length)
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
    if(CALL(self->fd, read, buffer, *length) != *length) {
      RaiseError(EIOError, "Unable to read %d bytes from %s@%lld", *length, 
		 self->fd->name, self->fd->readptr);
      goto error;
    };

  } else {
    RaiseError(EInvalidParameter, "Compression type %X unknown", compression_method);
    goto error;
  };

  // Here we have a good buffer - now calculate the crc32
  if(0){
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (unsigned char *)buffer,
		zip->cd_header.file_size);

    if(crc != zip->cd_header.crc32) {
      RaiseError(EIOError, 
		 "CRC not matched on decompressed file %s", zip->filename);
      goto error;
    };
  };

  /** Ok we get here we have good data - so we cache it */
  CALL(self->file_data_cache, put, talloc_strdup(buffer, zip->filename),
       buffer, *length);

  // Make sure we return the file to the position we found it in
  CALL(self->fd,seek, original_offset, SEEK_SET);
  return buffer;
 error:
  CALL(self->fd,seek, original_offset, SEEK_SET);
  return NULL;
};

/** file must be opened for writing */
static void ZipFile_create_new_volume(ZipFile self, FileLikeObject file) {
  // FIXME - close the old FD if thats still open
  if(self->fd) CALL(self, close);
  
  self->directory_offset=CALL(file, tell);
  self->fd = file;
  // We own it now.
  talloc_steal(self, file);
};

static FileLikeObject ZipFile_open_member(ZipFile self, char *filename, char mode,
				   char *extra, uint16_t extra_field_len,
				   int compression) {
  switch(mode) {
  case 'w': {
    struct ZipFileHeader header;

    if(self->writer) {
      RaiseError(ERuntimeError, "Unable to create a new member for writing '%s', when one is already writing '%s'", filename, self->writer->super.urn);
      return NULL;
    };

    CALL(self->fd, seek, self->directory_offset, SEEK_SET);
    // Write a file header on
    memset(&header, 0, sizeof(header));
    header.magic = 0x4034b50;
    header.version = 0x14;
    // We prefer to write trailing directory structures
    header.flags = 0x08;
    header.compression_method = compression;
    header.file_name_length = strlen(filename);
    header.extra_field_len = extra_field_len + 4;
    
    CALL(self->fd, write,(char *)&header, sizeof(header));
    CALL(self->fd, write, ZSTRING_NO_NULL(filename));
    CALL(self->fd, write, ZSTRING_NO_NULL("AF"));
    CALL(self->fd, write, (char *)&extra_field_len, sizeof(extra_field_len));
    CALL(self->fd, write, extra, extra_field_len);

    self->writer = (FileLikeObject)CONSTRUCT(ZipFileStream, 
					     ZipFileStream, Con, self, self,
					     self->fd, filename, 'w', compression,
					     self->directory_offset);
    self->_didModify = 1;
    break;
  };
  case 'r': {
    ZipInfo zip;
    struct ZipFileHeader header;
    //    char *extra;

    if(compression != ZIP_STORED) {
      RaiseError(ERuntimeError, "Unable to open seekable member for compressed members.");
      break;
    };

    zip = CALL(self, fetch_ZipInfo, filename);
    CALL(self->fd, seek, zip->cd_header.relative_offset_local_header, SEEK_SET);
    
    // Read the file header
    CALL(self->fd, read, (char *)&header, sizeof(header));

    // Skip the filename (bit extra for null termination):
    filename = talloc_size(self, header.file_name_length + 1);
    CALL(self->fd, read, filename, header.file_name_length);


    // We dont use the extra field for anything any more
    CALL(self->fd, seek, header.extra_field_len, SEEK_CUR);
#if 0
    // Read the extra field so we can cache it
    extra = talloc_size(self, header.extra_field_len);
    {
      char sig[2];
      uint16_t extra_field_len;

      CALL(self->fd, read, sig, sizeof(sig));
      CALL(self->fd, read, (char *)&extra_field_len, sizeof(extra_field_len));
      if(sig[0]=='A' && sig[1]=='F') {
	CALL(self->fd, read, extra, extra_field_len);

	// Cache it
	CALL(self->extra_cache, put, filename, extra, header.extra_field_len);
      };
    };
#endif

    self->writer = (FileLikeObject)CONSTRUCT(ZipFileStream, 
					     ZipFileStream, Con, self, self,
					     self->fd, filename, 'r', compression,
					     self->directory_offset);    
    self->writer->size = zip->cd_header.file_size;
    break;
  };
  default:
    RaiseError(ERuntimeError, "Unsupported mode '%c'", mode);
    return NULL;
  };

  return self->writer;
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

    CALL(self->fd, seek, self->directory_offset, SEEK_SET);
    
    // We rely on the fact that zipinfo_cache does not expire so we
    // can iterate over all ZipInfos by running over the cache_list
    list_for_each_entry(i, &self->zipinfo_cache->cache_list, cache_list) {
      ZipInfo j = (ZipInfo)i->data;

      // Its stored in this volume
      if(j->fd == self->fd) {
	// Write the CD file entry
	CALL(j->fd, write, (char *)&j->cd_header, sizeof(j->cd_header));
	CALL(j->fd, write, ZSTRING_NO_NULL(j->filename));
	k++;
      };
    };

    // Now write an end of central directory record
    memset(&end, 0, sizeof(end));
    end.magic = 0x6054b50;
    end.offset_of_cd = self->directory_offset;
    end.total_entries_in_cd_on_disk = k;
    end.total_entries_in_cd = k;
    end.size_of_cd = CALL(self->fd, tell) - self->directory_offset;
    
    CALL(self->fd, write, (char *)&end, sizeof(end));
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
     VMETHOD(create_new_volume) = ZipFile_create_new_volume;
     VMETHOD(open_member) = ZipFile_open_member;
     VMETHOD(close) = ZipFile_close;
     VMETHOD(writestr) = ZipFile_writestr;
     VMETHOD(fetch_ZipInfo) = ZipFile_fetch_ZipInfo;
     VMETHOD(add_zipinfo_to_cache) = ZipFile_add_zipinfo_to_cache;
END_VIRTUAL


static ZipFileStream ZipFileStream_Con(ZipFileStream self, ZipFile zip, 
				FileLikeObject fd, char *filename,
				char mode, int compression, 
				uint64_t header_offset) {
  self->zip = zip;
  self->mode = mode;
  self->super.super.urn = filename;

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

  self->zip_offset = CALL(fd, tell);
  self->zinfo = CONSTRUCT(ZipInfo, ZipInfo, Con, self, fd);
  
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

  // Update the crc:
  this->zinfo->cd_header.crc32 = crc32(this->zinfo->cd_header.crc32, 
				       (unsigned char*)buffer,
				       length);

  /** Position our write pointer */
  CALL(this->zinfo->fd, seek, this->zip_offset + self->readptr, SEEK_SET);
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
      ret = CALL(this->zinfo->fd, write, (char *)compressed, 
		 BUFF_SIZE - this->strm.avail_out);
      if(ret<0) return ret;
      result += ret;
    } while(this->strm.avail_out == 0);

  } else {
    /** Without compression, we just write the buffer right away */
    result = CALL(this->zinfo->fd, write, buffer, length);
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

  /** Position our write pointer */
  CALL(this->zinfo->fd, seek, this->zip_offset + self->readptr, SEEK_SET);
  len = CALL(this->zinfo->fd, read, buffer, length);
  self->readptr += len;
  return len;
};

static void ZipFileStream_close(FileLikeObject self) {
  ZipFileStream this = (ZipFileStream)self;
  
  if(this->mode == 'r') return;
  
  if(this->zinfo->cd_header.compression_method == ZIP_DEFLATE) {
    unsigned char compressed[BUFF_SIZE];

    do {
      int ret;

      this->strm.avail_out = BUFF_SIZE;
      this->strm.next_out = compressed;
      
      ret = deflate(&this->strm, Z_FINISH);
      ret = CALL(this->zinfo->fd, write, (char *)compressed, 
		 BUFF_SIZE - this->strm.avail_out);
      
      this->zinfo->cd_header.compress_size += ret;
    } while(this->strm.avail_out == 0);
    
    (void)deflateEnd(&this->strm);
 };

  // Update the length in zinfo:
  this->zinfo->cd_header.file_size = self->size;

  CALL(this->zinfo->fd, write, (char *)&this->zinfo->cd_header.crc32,
       sizeof(this->zinfo->cd_header.crc32));
  CALL(this->zinfo->fd, write, (char *)&this->zinfo->cd_header.compress_size,
       sizeof(uint32_t));
  CALL(this->zinfo->fd, write, (char *)&this->zinfo->cd_header.file_size,
       sizeof(uint32_t));

  // Now the zinfo is complete we can add it to the volume cache:
  CALL(this->zip->zipinfo_cache, put, 
       this->zinfo->filename, this->zinfo, sizeof(*this->zinfo));

  // Make this is the point where we will be writing the next file.
  this->zip->directory_offset = CALL(this->zinfo->fd, tell);

  // Remove ourselves from the parent - its safe to open a new stream
  // now:
  this->zip->writer = NULL;

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
