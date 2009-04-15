#include "zip.h"

/*************************************************************
  The Image stream works by collecting chunks into segments. Chunks
  are compressed seperately using zlib's compress function.

  Defined attributes:

  aff2:type                "image"
  aff2:chunk_size          The size of the chunk in bytes (32k)
  aff2:chunks_in_segment   The number of chunks in each segment (2048)
  aff2:stored              The URN of the object which stores this
                           stream - This must be a "volume" object
  aff2:size                The size of this stream in bytes (0)

  Note that segments are "blob" objects with an implied URN of:

  "%s/%08d" % (Image.urn, segment_number)

**************************************************************/


// The segments created by the Image stream are named by segment
// number with this format.
#define IMAGE_SEGMENT_NAME_FORMAT "%s/%08d"  /** Stream URN, segment_id */

/** This specialised hashing is for integer keys */
static unsigned int cache_hash_int(Cache self, void *key) {
  uint32_t int_key = *(uint32_t *)key;

  return int_key % self->hash_table_width;
};

static int cache_cmp_int(Cache self, void *other) {
  uint32_t int_key = *(uint32_t *)self->key;
  uint32_t int_other = *(uint32_t *)other;  

  return int_key != int_other;
};

static int Image_destructor(void *this) {
  Image self = (Image)this;

  CALL((FileLikeObject)self, close);

  return 0;
};

static AFFObject Image_Con(AFFObject self, char *uri, char mode) {
  Image this=(Image)self;
  char *value;

  if(uri) {
    self->urn = talloc_strdup(self, uri);

    // These are the essential properties:
    value = resolver_get_with_default(oracle, self->urn, AFF4_CHUNK_SIZE, "32k");
    this->chunk_size = parse_int(value);

    value = resolver_get_with_default(oracle, self->urn, AFF4_COMPRESSION, "8");
    this->compression = parse_int(value);
    
    value = resolver_get_with_default(oracle, self->urn, AFF4_CHUNKS_IN_SEGMENT, "2048");
    this->chunks_in_segment = parse_int(value);

    value = resolver_get_with_default(oracle, self->urn, AFF4_SIZE, "0");
    this->super.size = parse_int(value);

    this->parent_urn = CALL(oracle, resolve, URNOF(this), AFF4_STORED);
    if(this->parent_urn)
      this->parent_urn = talloc_strdup(self, this->parent_urn);

    this->chunk_buffer = talloc_size(self, this->chunk_size);

    this->segment_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
    // This is a hack to make us pre-allocate the buffer
    //CALL(this->segment_buffer, seek, this->chunk_size * this->chunks_in_segment, SEEK_SET);
    //CALL(this->segment_buffer, seek, 0, SEEK_SET);

    this->chunk_count = 0;
    this->segment_count =0;
    
    this->chunk_indexes = talloc_array(self, int32_t, this->chunks_in_segment + 2);
    // Fill it with -1 to indicate an invalid pointer
    memset(this->chunk_indexes, 0xff, sizeof(int32_t) * this->chunks_in_segment);

    // Initialise the chunk cache:
    this->chunk_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, CACHE_SIZE);

    // We need to use slightly different hashing and cmp operations as
    // we will be using the chunk_id (int) as a key:
    this->chunk_cache->hash = cache_hash_int;
    this->chunk_cache->cmp = cache_cmp_int;
  } else {
    // Call our baseclass
    this->__super__->super.Con(self, uri, mode);
  };

  // NOTE - its a really bad idea to set destructors which call
  // close() because they might try to reaccess the cache, which may
  // be invalid while we get free (basically a reference cycle).  The
  // downside is that we insist people call close() explicitely.
  // talloc_set_destructor(self, Image_destructor);
  return self;
};
  
static AFFObject Image_finish(AFFObject self) {
  Image this = (Image)self;

  // Make sure the oracle knows we are an image
  CALL(oracle, set, self->urn, AFF4_TYPE, AFF4_IMAGE);
  EVP_DigestInit(&this->digest, EVP_sha256());
  return CALL(self, Con, self->urn, 'w');
};

/** This is how we implement the Image stream writer:

As new data is written we append it to the chunk buffer, then we
remove chunk sized buffers from it and push them to the segment. When
the segment is full we dump it to the parent container set.
**/
static int dump_chunk(Image this, char *data, uint32_t length, int force) {
  // We just use compress() to get the compressed buffer.
  char cbuffer[2*compressBound(length)];
  int clength=2*compressBound(length);

  // Should we offer to store chunks uncompressed?
  if(this->compression == 0) {
    memcpy(cbuffer, data, length);
    clength = length;
  } else {
    if(compress((unsigned char *)cbuffer, (unsigned long int *)&clength, 
		(unsigned char *)data, (unsigned long int)length) != Z_OK) {
      RaiseError(ERuntimeError, "Compression error");
      return -1;
    };
  };
  
  // Update the index to point at the current segment stream buffer
  // offset
  this->chunk_indexes[this->chunk_count] = this->segment_buffer->readptr;
  
  // Now write the buffer to the stream buffer
  CALL(this->segment_buffer, write, cbuffer, clength);
  this->chunk_count ++;
  
  // Is the segment buffer full? If it has enough chunks in it we
  // can flush it out. We need to find out where our storage is and
  // open a ZipFile on it:
  if(this->chunk_count >= this->chunks_in_segment || force) {
    char tmp[BUFF_SIZE];
    ZipFile parent;

    if(!this->parent_urn) {
      RaiseError(ERuntimeError, "No storage for Image stream?");
      goto error;
    }

    parent  = (ZipFile)CALL(oracle, open, this, this->parent_urn, 'w');
    // Format the segment name
    snprintf(tmp, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, ((AFFObject)this)->urn, 
	     this->segment_count);

    // Push one more offset to the index to cover the last chunk
    this->chunk_indexes[this->chunk_count + 1] = this->segment_buffer->readptr;

    printf("Dumping segment %s (%lld bytes)\n", tmp, this->segment_buffer->readptr);

    // Store the entire segment in the zip file
    CALL((ZipFile)parent, writestr,
	 tmp, this->segment_buffer->data, 
	 this->segment_buffer->readptr, 
	 NULL, 0,
	 // No compression for segments
	 ZIP_STORED
	 );

    // Now write the index file which accompanies the segment
    snprintf(tmp, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT ".idx", 
	     ((AFFObject)this)->urn, 
	     this->segment_count);

    CALL((ZipFile)parent, writestr,
	 tmp, (char *)this->chunk_indexes,
	 (this->chunk_count + 1) * sizeof(uint32_t),
	 NULL, 0,
	 ZIP_STORED
	 );

    // Done with parent
    CALL(oracle, cache_return, (AFFObject)parent);
	 
    // Reset everything to the start
    CALL(this->segment_buffer, truncate, 0);
    memset(this->chunk_indexes, -1, sizeof(int32_t) * this->chunks_in_segment);
    this->chunk_count =0;
    // Next segment
    this->segment_count ++;
  };
  
  return clength;
 error:
  return -1;
};

static int Image_write(FileLikeObject self, char *buffer, unsigned long int length) {
  Image this = (Image)self;
  int available_to_read;
  int buffer_readptr=0;

  EVP_DigestUpdate(&this->digest, buffer, length);

  while(buffer_readptr < length) {
    available_to_read = min(this->chunk_size - this->chunk_buffer_readptr, 
			    length - buffer_readptr);
    memcpy(this->chunk_buffer + this->chunk_buffer_readptr,
	   buffer + buffer_readptr, available_to_read);
    this->chunk_buffer_readptr += available_to_read;

    if(this->chunk_buffer_readptr == this->chunk_size) {
      if(dump_chunk(this, this->chunk_buffer, this->chunk_size, 0)<0)
	return -1;
      this->chunk_buffer_readptr = 0;
    };

    buffer_readptr += available_to_read;
    self->readptr += available_to_read;
  };
  
  self->size = max(self->readptr, self->size);

  return length;
};

static void Image_close(FileLikeObject self) {
  Image this = (Image)self;
  unsigned char buff[BUFF_SIZE];
  unsigned int len;
  unsigned char hash_base64[BUFF_SIZE];

  // Write the last chunk
  dump_chunk(this, this->chunk_buffer, this->chunk_buffer_readptr, 1);
  dump_stream_properties(self, this->parent_urn);

  EVP_DigestFinal(&this->digest, buff, &len);
  encode64(buff, len, hash_base64);
  CALL(oracle, set, URNOF(self), AFF4_SHA, (char *)hash_base64);

  this->__super__->close(self);
};

/** Reads at most a single chunk and write to result. Return how much
    data was actually read.
*/
static int partial_read(FileLikeObject self, StringIO result, int length) {
  Image this = (Image)self;

  // which segment is it?
  uint32_t chunk_id = self->readptr / this->chunk_size;
  int segment_id = chunk_id / this->chunks_in_segment;
  int chunk_index_in_segment = chunk_id % this->chunks_in_segment;
  int chunk_offset = self->readptr % this->chunk_size;
  int available_to_read = min(this->chunk_size - chunk_offset, length);

  /* Temporary storage for the compressed chunk */
  char compressed_chunk[this->chunk_size + 1024];
  unsigned int compressed_length;

  /* Temporary storage for the uncompressed chunk */
  char *uncompressed_chunk;
  unsigned int uncompressed_length=this->chunk_size + 1024;

  /** Now we need to figure out where the segment is */
  char buffer[BUFF_SIZE];
  ZipFile parent;
  FileLikeObject fd;
  int32_t chunk_offset_in_segment;

  /** Fast path - check if the chunk is already cached.  If it is - we
  can just copy a subset of it on the result and get out of here....
  Note that we maintain a seperate cache of chunks because if we
  cached the entire segment (which could be very large) we would
  exhaust our cache memory very quickly.
  */
  Cache chunk_cache = CALL(this->chunk_cache, get, &chunk_id);
  if(chunk_cache) {
    available_to_read = min(available_to_read, chunk_cache->data_len);

    // Copy it on the result stream
    length = CALL(result, write, chunk_cache->data + chunk_offset, 
		  available_to_read);
    
    self->readptr += length;

    return length;
  };

  // Make some memory on the heap - it will be stolen by the cache
  uncompressed_chunk = talloc_size(self, uncompressed_length);
  
  /** First we need to locate the chunk indexes */
  snprintf(buffer, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT ".idx", 
	   ((AFFObject)this)->urn, 
	   segment_id);

  // Now we work out the offsets on the chunk in the segment - we read
  // the segment index which is a blob:
  {
    FileLikeObject fd = (FileLikeObject)CALL(oracle, open, self, buffer, 'r');
    int32_t *chunk_index;
    int chunks_in_segment;

    if(!fd) {
      RaiseError(ERuntimeError, "Unable to locate index %s", buffer);
      goto error;
    };
    
    /** This holds the index into the segment of all the chunks.
	It is an array of chunks_in_segment ints long.
    */
    chunk_index = (int32_t *)CALL(fd, get_data);
    chunks_in_segment = fd->size / sizeof(int32_t);
    
    /** By here we have the chunk_index which is an array of offsets
	into the segment where the chunks begin. We work out the size of
	the chunk by subtracting the next chunk offset from the current
	one. It seems to be ok to over read here so if the numbers dont
	make too much sense we just choose to overread.
    */
    compressed_length = min((uint32_t)chunk_index[chunk_index_in_segment+1] -
			    (uint32_t)chunk_index[chunk_index_in_segment], 
			    this->chunk_size + 1024);
    
    /** Now obtain a handler directly into the segment */
    snprintf(buffer, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT,
	     ((AFFObject)this)->urn, 
	     segment_id);
    
    chunk_offset_in_segment = (uint32_t)chunk_index[chunk_index_in_segment];
    CALL(oracle, cache_return, (AFFObject)fd);
  };

  // Now we need to read the ZipFile directly
  parent = (ZipFile)CALL(oracle, open, self, this->parent_urn, 'r');
  if(!parent) {
    RaiseError(ERuntimeError, "No storage for Image stream?");
    goto error;
  }

  // We want to open a direct filehandle to the segment because we do
  // not want to cache it - it will invalidate the cache too much if
  // we do.
  fd = CALL((ZipFile)parent, open_member, buffer, 'r', NULL, 0, ZIP_STORED);

  // Fetch the compressed chunk
  CALL(fd, seek, chunk_offset_in_segment, SEEK_SET);

  //  printf("compressed_length %d %d ", compressed_length, uncompressed_length);
  CALL(fd, read, compressed_chunk, compressed_length);

  // Done with parent and blob
  CALL(oracle, cache_return, (AFFObject)parent);
  CALL(fd, close);

  if(this->compression == 8) {
    // Try to decompress it:
    if(uncompress((unsigned char *)uncompressed_chunk, 
		  (unsigned long int *)&uncompressed_length, 
		  (unsigned char *)compressed_chunk,
		  (unsigned long int)compressed_length) != Z_OK ) {
      RaiseError(ERuntimeError, "Unable to decompress chunk %d", chunk_id);
      goto error;
    };
  } else {
    memcpy(uncompressed_chunk, compressed_chunk, compressed_length);
    uncompressed_length = compressed_length;
  };
  
  //  printf("%d\n", uncompressed_length);
  // Copy it on the output stream
  length = CALL(result, write, uncompressed_chunk + chunk_offset, 
		available_to_read);

  self->readptr += available_to_read;

  // OK - now cache the uncompressed_chunk (this will steal it so we
  // dont need to free it):
  CALL(this->chunk_cache, put, talloc_memdup(NULL, &chunk_id, sizeof(chunk_id)),
       uncompressed_chunk,
       uncompressed_length);

  return length;

  error: {
    char pad[available_to_read];

    memset(pad,0, this->chunk_size);
    length = CALL(result, write, pad, available_to_read);

    self->readptr += available_to_read;
    talloc_free(uncompressed_chunk);
    return length;
  };
};

// Reads from the image stream
static int Image_read(FileLikeObject self, char *buffer, unsigned long int length) {
  StringIO result = CONSTRUCT(StringIO, StringIO, Con, self);
  int read_length;
  int len = 0;

  // Clip the read to the stream size
  length = min(length, self->size - self->readptr);

  // Just execute as many partial reads as are needed to satisfy the
  // length requested
  while(len < length ) {
    read_length = partial_read(self, result, length - len);
    if(read_length <=0) break;
    len += read_length;
  };

  CALL(result, seek, 0, SEEK_SET);
  read_length = CALL(result, read, buffer, length);

  talloc_free(result);

  return len;
};

void dump_stream_properties(FileLikeObject self, char *volume) {
  char tmp[BUFF_SIZE];
  char *properties;

  // Set the stream size
  snprintf(tmp, BUFF_SIZE, "%lld", self->size);
  CALL(oracle, set, ((AFFObject)self)->urn, AFF4_SIZE, tmp);

  // Write out a properties file
  properties = CALL(oracle, export_urn, URNOF(self), URNOF(self));
  if(properties) {
    ZipFile zipfile = (ZipFile)CALL(oracle, open, self, volume, 'w');

    snprintf(tmp, BUFF_SIZE, "%s/properties", ((AFFObject)self)->urn);
    CALL((ZipFile)zipfile, writestr, tmp, ZSTRING_NO_NULL(properties),
	 NULL, 0, ZIP_STORED);

    talloc_free(properties);
    // Done with zipfile
    CALL(oracle, cache_return, (AFFObject)zipfile);
  };
};


VIRTUAL(Image, FileLikeObject)
     VMETHOD(super.super.Con) = Image_Con;
     VMETHOD(super.super.finish) = Image_finish;
     VMETHOD(super.read) = Image_read;
     VMETHOD(super.write) = Image_write;
     VMETHOD(super.close) = Image_close;
END_VIRTUAL

