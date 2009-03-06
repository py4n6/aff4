#include "fif.h"
#include "zip.h"

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

static AFFObject Image_Con(AFFObject self, char *uri) {
  Image this=(Image)self;
  char *value;

  if(uri) {
    self->urn = talloc_strdup(self, uri);

    // These are the essential properties:
    value = resolver_get_with_default(oracle, self->urn, "aff2:chunk_size", "32k");
    this->chunk_size = parse_int(value);
    
    value = resolver_get_with_default(oracle, self->urn, "aff2:chunks_in_segment", "2048");
    this->chunks_in_segment = parse_int(value);

    value = resolver_get_with_default(oracle, self->urn, "aff2:size", "0");
    this->super.size = parse_int(value);

    // Make sure the oracle knows we are an image
    CALL(oracle, add, self->urn, "aff2:type", "image");

    this->chunk_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
    this->segment_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
    this->chunk_count = 0;
    this->segment_count =0;
    
    this->chunk_indexes = talloc_array(self, int32_t, this->chunks_in_segment);
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
    this->__super__->super.Con(self, uri);
  };

  return self;
};
  
AFFObject Image_finish(AFFObject self) {
  return CALL(self, Con, self->urn);
};

/** This is how we implement the Image stream writer:

As new data is written we append it to the chunk buffer, then we
remove chunk sized buffers from it and push them to the segment. When
the segment is full we dump it to the parent container set.
**/
static int dump_chunk(Image this, char *data, uint32_t length, int force) {
  // We just use compress() to get the compressed buffer.
  char cbuffer[compressBound(length)];
  int clength=compressBound(length);

  memset(cbuffer,0,clength);

  // Should we even offer to store chunks uncompressed?
  if(0) {
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
  // open a FIFFile on it:
  if(this->chunk_count >= this->chunks_in_segment || force) {
    char tmp[BUFF_SIZE];
    char *parent_urn = CALL(oracle, resolve, ((AFFObject)this)->urn, "aff2:stored");
    FIFFile parent;

    if(!parent_urn) {
      RaiseError(ERuntimeError, "No storage for Image stream?");
      goto error;
    }

    parent  = (FIFFile)CALL(oracle, open, parent_urn);
    // Format the segment name
    snprintf(tmp, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, ((AFFObject)this)->urn, 
	     this->segment_count);

    // Push one more offset to the index to cover the last chunk
    this->chunk_indexes[this->chunk_count + 1] = this->segment_buffer->readptr;

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

  CALL(this->chunk_buffer, seek, 0, SEEK_END);
  CALL(this->chunk_buffer, write, buffer, length);
  while(this->chunk_buffer->readptr > this->chunk_size) {
    if(dump_chunk(this, this->chunk_buffer->data, this->chunk_size, 0)<0)
      return -1;
      
    // Consume the chunk from the chunk_buffer
    CALL(this->chunk_buffer, skip, this->chunk_size);
  };

  self->readptr += length;
  self->size = max(self->readptr, self->size);

  return length;
};

static void Image_close(FileLikeObject self) {
  Image this = (Image)self;
  char tmp[BUFF_SIZE];
  char *properties;

  // Write the last chunk
  dump_chunk(this, this->chunk_buffer->data, this->chunk_buffer->readptr, 1);

  // Set the stream size
  snprintf(tmp, BUFF_SIZE, "%lld", self->size);
  CALL(oracle, set, ((AFFObject)self)->urn, "aff2:size", tmp);

  // Write out a properties file
  properties = CALL(oracle, export, ((AFFObject)self)->urn);
  if(properties) {
    char *parent_urn = CALL(oracle, resolve, ((AFFObject)this)->urn, "aff2:stored");
    FIFFile fiffile = (FIFFile)CALL(oracle, open, parent_urn);

    snprintf(tmp, BUFF_SIZE, "%s/properties", ((AFFObject)self)->urn);
    CALL((ZipFile)fiffile, writestr, tmp, ZSTRING_NO_NULL(properties),
	 NULL, 0, ZIP_STORED);

    talloc_free(properties);
  };

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
  char *parent_urn;
  FIFFile parent;
  Blob temp_blob;
  int32_t *chunk_index;
  int chunks_in_segment;
  FileLikeObject fd;

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

  // No we need to read the FIFFile directly
  parent_urn = CALL(oracle, resolve, ((AFFObject)this)->urn, "aff2:stored");
  parent = (FIFFile)CALL(oracle, open, parent_urn);
  if(!parent) {
    RaiseError(ERuntimeError, "No storage for Image stream?");
    goto error;
  }
  
  /** First we need to locate the chunk indexes */
  snprintf(buffer, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT ".idx", 
	   ((AFFObject)this)->urn, 
	   segment_id);

  temp_blob = (Blob)CALL(oracle, open, buffer);
  if(!temp_blob) {
    RaiseError(ERuntimeError, "Unable to locate index %s", buffer);
    goto error;
  };

  /** This holds the index into the segment of all the chunks.
      It is an array of chunks_in_segment ints long.
  */
  chunk_index = (int32_t *)temp_blob->data;
  chunks_in_segment = temp_blob->length / sizeof(int32_t);
  
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
  
  fd = CALL((ZipFile)parent, open_member, buffer, 'r', NULL, 0, ZIP_STORED);

  // Fetch the compressed chunk
  CALL(fd, seek, chunk_index[chunk_index_in_segment],
       SEEK_SET);

  CALL(fd, read, compressed_chunk, compressed_length);

  // Try to decompress it:
  if(uncompress((unsigned char *)uncompressed_chunk, 
		(unsigned long int *)&uncompressed_length, 
		(unsigned char *)compressed_chunk,
		(unsigned long int)compressed_length) != Z_OK ) {
    RaiseError(ERuntimeError, "Unable to decompress chunk %d", chunk_id);
    goto error;
  };

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

VIRTUAL(Image, FileLikeObject)
     VMETHOD(super.super.Con) = Image_Con;
     VMETHOD(super.super.finish) = Image_finish;
     VMETHOD(super.read) = Image_read;
     VMETHOD(super.write) = Image_write;
     VMETHOD(super.close) = Image_close;
END_VIRTUAL

