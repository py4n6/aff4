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

/** Implementation of FileBackedObject.

FileBackedObject is a FileLikeObject which uses a read file to back
itself.
*/
int close_fd(void *self) {
  FileBackedObject this = (FileBackedObject)self;
  close(this->fd);
  return 0;
};

FileBackedObject FileBackedObject_con(FileBackedObject self, 
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

  return self;
};

uint64_t FileLikeObject_seek(FileLikeObject self, int64_t offset, int whence) {
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
int FileBackedObject_read(FileLikeObject self, char *buffer, int length) {
  FileBackedObject this = (FileBackedObject)self;
  int result;

  lseek(this->fd,self->readptr,0);
  result = read(this->fd, buffer, length);
  if(result < 0) {
    RaiseError(EIOError, "Unable to read from %s (%s)", self->name, strerror(errno));
  };

  self->readptr += result;

  return result;
};

int FileBackedObject_write(FileLikeObject self, char *buffer, int length) {
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

uint64_t FileLikeObject_tell(FileLikeObject self) {
  return self->readptr;
};

VIRTUAL(FileLikeObject, Object)
     VMETHOD(seek) = FileLikeObject_seek;
     VMETHOD(tell) = FileLikeObject_tell;
END_VIRTUAL

/** A file backed object extends FileLikeObject */
VIRTUAL(FileBackedObject, FileLikeObject)
     VMETHOD(con) = FileBackedObject_con;
     VMETHOD(super.read) = FileBackedObject_read;
     VMETHOD(super.write) = FileBackedObject_write;
END_VIRTUAL;

int ZipInfo_destructor(void *self) {
  ZipInfo this = (ZipInfo)self;

  // Make sure we remove ourselves from the list when we get freed
  list_del(&this->list);

  return 0;
};

/** Now implement Zip file support */
ZipInfo ZipInfo_Con(ZipInfo self, FileLikeObject fd) {
  self->fd = fd;
  INIT_LIST_HEAD(&self->list);
  INIT_LIST_HEAD(&self->cache_list);

  return self;
};

VIRTUAL(ZipInfo, Object)
     VMETHOD(Con) = ZipInfo_Con;
END_VIRTUAL

#define HASH_TABLE_SIZE 256

/** Quick and simple */
int hash(char *name, int len) {
  char result = 0;
  int i;
  for(i=0; i<len; i++) 
    result ^= name[i];

  return result % HASH_TABLE_SIZE;
};

// Add the ZipInfo class into our internal hash table... Note that we
// also steal the memory so callers should not free it.
void add_to_hash(ZipFile self, ZipInfo zip) {
  int index = hash(zip->cd_header.filename, zip->cd_header.file_name_length);

  if(!self->hash_table[index]) {
    self->hash_table[index] = CONSTRUCT(ZipInfo, ZipInfo, Con, self->hash_table, NULL);
  };

  // Just add to the list for now
  list_add_tail(&zip->list, &self->hash_table[index]->list);

  // Steal the zip
  talloc_steal(self->hash_table, zip);
};

/** Fetch the ZipInfo from the hashtable - note that callers no longer
    own it - they must not free it.
*/
ZipInfo fetch_from_hashtable(ZipInfo *hashtable, char *filename) {
  ZipInfo zip;
  int index = hash(ZSTRING(filename));

  if(hashtable[index]) {
    list_for_each_entry(zip, &(hashtable[index]->list), list) {
      if(zip->cd_header.filename && !memcmp(filename, zip->cd_header.filename,
					    zip->cd_header.file_name_length))
	return zip;
    };
  };

  RaiseError(EInvalidParameter, "Archive member %s not found", filename);
  return NULL;
};

ZipFile ZipFile_Con(ZipFile self, FileLikeObject fd) {
  char buffer[4096];
  int length,i;
  self->fd = fd;

  // Initialise the hash table:
  self->hash_table = talloc_array(self, ZipInfo, HASH_TABLE_SIZE);

  // Make a new cache
  self->max_cache_size = 50;
  self->cache_size = 0;
  self->cache_head = CONSTRUCT(ZipInfo, ZipInfo, Con, self, NULL);

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

    // The central directory starts at the current offset less the
    // specified offset
    CALL(self->fd, seek, self->end->offset_of_cd, SEEK_SET);

    while(i<self->end->total_entries_in_cd_on_disk) {
      ZipInfo zip = CONSTRUCT(ZipInfo, ZipInfo, Con, self, self->fd);
      // The length of the struct up to the filename
      int length = ((char *)&zip->cd_header.filename - 
		    (char *)&zip->cd_header);

      // Only read up to the filename member
      if(length != CALL(self->fd, read, (char *)&zip->cd_header, length))
	goto error_reason;

      // Does the magic match?
      if(zip->cd_header.magic != 0x2014b50)
	goto error_reason;

      // Now read the filename
      length = zip->cd_header.file_name_length;
      zip->cd_header.filename = (char *)talloc_size(self, length);
						
      if(CALL(self->fd, read, zip->cd_header.filename, length) == 0)
	goto error_reason;

      // Add to our hashtable
      add_to_hash(self, zip);

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

int ZipFile_read_member(ZipFile self, char *filename, 
			char **buffer, int *length) {
  ZipInfo zip = fetch_from_hashtable(self->hash_table, filename);
  char *result;
  char compression_method;
  struct ZipFileHeader header;
  int read_length = sizeof(header);
  if(!zip) return -1;

  // We know how large we would like the buffer so we make it that big
  if(*length < zip->cd_header.file_size) {
    *buffer = talloc_realloc_size(self, *buffer, zip->cd_header.file_size);
  };

  *length = zip->cd_header.file_size;

  // Does this ZipInfo have a cache? If so just return that
  if(zip->cached_data) {
    // Move it to the tail of the list
    list_del(&zip->cache_list);
    list_add_tail(&zip->cache_list, &self->cache_head->cache_list);

    memcpy(*buffer, zip->cached_data, *length);
    return *length;
  };

  compression_method = zip->cd_header.compression_method;

  // Read the File Header
  CALL(self->fd, seek, zip->cd_header.relative_offset_local_header,
       SEEK_SET);
  if(CALL(self->fd, read,(char *)&header, read_length) != read_length)
    goto error;
  
  // Check the magic
  if(header.magic != 0x4034b50) {
    RaiseError(ERuntimeError, "Unable to file file header");
    goto error;
  };

  // Skip the filename and extra data
  CALL(self->fd, seek, header.file_name_length + header.extra_field_len, SEEK_CUR);
  
  if(compression_method == ZIP_DEFLATE) {
    int compressed_length = zip->cd_header.compress_size;
    int uncompressed_length = *length;
    char *tmp = talloc_size(*buffer, compressed_length);
    z_stream strm;

    memset(&strm, 0, sizeof(strm));

    //Now read the data in
    if(CALL(self->fd, read, tmp, compressed_length) != compressed_length)
      goto error;

    // Decompress it
    /** Set up our decompressor */
    strm.next_in = (unsigned char *)tmp;
    strm.avail_in = compressed_length;
    strm.next_out = (unsigned char *)*buffer;
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
    if(CALL(self->fd, read, *buffer, *length) != *length) {
      RaiseError(EIOError, "Unable to read %d bytes from %s@%lld", *length, 
		 self->fd->name, self->fd->readptr);
      goto error;
    };

  } else {
    RaiseError(EInvalidParameter, "Compression type %X unknown", compression_method);
    goto error;
  };

  // Here we have a good buffer - now calculate the crc32
  {
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, (unsigned char *)*buffer,
		zip->cd_header.file_size);

    if(crc != zip->cd_header.crc32) {
      RaiseError(EIOError, 
		 "CRC not matched on decompressed file %s", zip->cd_header.filename);
      goto error;
    };
  };

  /** Ok we get here we have good data - so we cache it */
  zip->cached_data = talloc_memdup(zip, *buffer, *length);
  list_add_tail(&zip->cache_list, &self->cache_head->cache_list);
  self->cache_size++;
  /** Is it time to expire the cache? Expire the cache from the
      front. This ensures we expire the oldest caches first.
  */
  while(self->cache_size >= self->max_cache_size) {
    ZipInfo first;
    list_next(first, &self->cache_head->cache_list, cache_list);

    list_del(&first->cache_list);
    if(first->cached_data) {
      talloc_free(first->cached_data);
      first->cached_data = NULL;
    };
    self->cache_size--;
  };

  return *length;
 error:
  return NULL;
};

/** file must be opened for writing */
void ZipFile_create_new_volume(ZipFile self, FileLikeObject file) {
  // FIXME - close the old FD if thats still open
  //if(self->fd) ...
  
  self->fd = file;
};

FileLikeObject ZipFile_open_member(ZipFile self, char *filename, char mode,
				   int compression) {
  switch(mode) {
  case 'w': {
    struct ZipFileHeader header;

    if(self->writer) {
      RaiseError(ERuntimeError, "Unable to create a new member for writing '%s', when one is already writing '%s'", filename, self->writer->name);
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
    
    CALL(self->fd, write,(char *)&header, sizeof(header));
    CALL(self->fd, write, ZSTRING_NO_NULL(filename));

    self->writer = (FileLikeObject)CONSTRUCT(ZipFileStream, 
					     ZipFileStream, Con, self, self,
					     self->fd, filename, 'w', compression,
					     self->directory_offset);
    break;
  };
  case 'r':
    


  default:
    RaiseError(ERuntimeError, "Unsupported mode '%c'", mode);
    return NULL;
  };

  return self->writer;
};

void ZipFile_close(ZipFile self) {
  // Dump the current CD. We expect our fd is seeked to the right
  // place:
  ZipInfo j;
  int i,k=0;
  ZipFileStream writer = (ZipFileStream)self->writer;

  // We iterate over all the ZipInfo hashes we have and see which ones
  // point at this file. We will need to be rewritten back to this
  // file:
  if(writer) {
    for(i=0; i<HASH_TABLE_SIZE; i++)
      if(self->hash_table[i]) {
	list_for_each_entry(j, &self->hash_table[i]->list, list) {
	  if(j->fd == writer->zinfo->fd) {
	    int length=(char *)&j->cd_header.filename - (char *)&j->cd_header;
	    // Write the CD file entry
	    CALL(j->fd, write, (char *)&j->cd_header, length);
	    CALL(j->fd, write, j->cd_header.filename,
		 j->cd_header.file_name_length);
	    k++;
	  };
	};
      };

    // Now write an end of central directory:
    {
      struct EndCentralDirectory end;
      memset(&end, 0, sizeof(end));
      end.magic = 0x6054b50;
      end.offset_of_cd = self->directory_offset;
      end.total_entries_in_cd_on_disk = k;
      end.total_entries_in_cd = k;
      end.size_of_cd = CALL(writer->zinfo->fd, tell) - self->directory_offset;

      CALL(writer->zinfo->fd, write, (char *)&end, sizeof(end));
    };
  };
};

VIRTUAL(ZipFile, Object)
     VMETHOD(Con) = ZipFile_Con;
     VMETHOD(read_member) = ZipFile_read_member;
     VMETHOD(create_new_volume) = ZipFile_create_new_volume;
     VMETHOD(open_member) = ZipFile_open_member;
     VMETHOD(close) = ZipFile_close;
END_VIRTUAL


ZipFileStream ZipFileStream_Con(ZipFileStream self, ZipFile zip, 
				FileLikeObject fd, char *filename,
				char mode, int compression, 
				uint64_t header_offset) {
  self->zip = zip;
  self->mode = mode;

  if(mode=='w') {
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
    cd->filename = talloc_strdup(self->zinfo, filename);
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
int ZipFileStream_write(FileLikeObject self, char *buffer, int length) {
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
    this->strm.next_in = buffer;
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

void ZipFileStream_close(FileLikeObject self) {
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

  add_to_hash(this->zip, this->zinfo);
  this->zip->directory_offset = CALL(this->zinfo->fd, tell);
};

VIRTUAL(ZipFileStream, FileLikeObject)
     VMETHOD(Con) = ZipFileStream_Con;
     VMETHOD(super.write) = ZipFileStream_write;
     VMETHOD(super.close) = ZipFileStream_close;
END_VIRTUAL
