#include "zip.h"
#include "encode.h"

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

  Note that bevies are segment objects with an implied URN of:

  "%s/%08d" % (Image.urn, bevy_number)

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

static ImageWorker ImageWorker_Con(ImageWorker self, Image image) {
  ImageWorker this=self;

  URNOF(self) = URNOF(image);

  this->image = image;
  this->bevy = CONSTRUCT(StringIO, StringIO, Con, self);
  
  this->segment_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  this->chunk_indexes = talloc_array(self, int32_t, image->chunks_in_segment + 2);

  // Fill it with -1 to indicate an invalid pointer
  memset(this->chunk_indexes, 0xff, sizeof(int32_t) * image->chunks_in_segment);

  return self;
};

// Compresses all the data in the current bevy and dumps it on the
// storage. The bevy must be as full as can be (image->bevy_size)
// before we dump it (except for the last bevy in the image which can
// be short).
static int dump_bevy(ImageWorker this) {
  int segment_number = this->segment_count;
  int bevy_index=0;
  int chunk_count=0;
  int result;

  while(bevy_index < this->bevy->size) {
    int length = min(this->image->chunk_size, this->bevy->size - bevy_index);
    // We just use compress() to get the compressed buffer.
    char cbuffer[2*compressBound(this->image->chunk_size)];
    int clength=2*compressBound(this->image->chunk_size);

    // Should we offer to store chunks uncompressed?
    if(this->image->compression == 0) {
      memcpy(cbuffer, this->bevy->data + bevy_index, length);
      clength = length;
    } else {
      if(compress2((unsigned char *)cbuffer, (unsigned long int *)(char *)&clength, 
		   (unsigned char *)this->bevy->data + bevy_index, (unsigned long int)length, 1) != Z_OK) {
	RaiseError(ERuntimeError, "Compression error");
	return -1;
      };
    };

    // Update the index to point at the current segment stream buffer
    // offset
    this->chunk_indexes[chunk_count] = this->segment_buffer->readptr;
    
    // Now write the buffer to the stream buffer
    CALL(this->segment_buffer, write, cbuffer, clength);
    chunk_count ++;
    bevy_index += length;
  };

  // Dump the bevy to the zip volume
  {
    char tmp[BUFF_SIZE];
    ZipFile parent;
    char *parent_urn = (char *)CALL(oracle, resolve, this, URNOF(this), AFF4_STORED,
				    RESOLVER_DATA_URN);

    if(!parent_urn) {
      RaiseError(ERuntimeError, "No storage for Image stream?");
      goto error;
    }

    parent  = (ZipFile)CALL(oracle, open, parent_urn, 'w');

    // Format the segment name
    snprintf(tmp, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, ((AFFObject)this)->urn, 
	     segment_number);
    
    // Push one more offset to the index to cover the last chunk
    this->chunk_indexes[chunk_count + 1] = this->segment_buffer->readptr;

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
	     segment_number);

    CALL((ZipFile)parent, writestr,
	 tmp, (char *)this->chunk_indexes,
	 (chunk_count + 1) * sizeof(uint32_t),
	 NULL, 0,
	 ZIP_STORED
	 );

    // Done with parent
    CALL(oracle, cache_return, (AFFObject)parent);
	 
    // Reset everything to the start
    CALL(this->segment_buffer, truncate, 0);
    CALL(this->bevy, truncate, 0);
    memset(this->chunk_indexes, -1, sizeof(int32_t) * this->image->chunks_in_segment);
  };
  
  
  result= 1;
 error:
  result = -1;

  // Return ourselves to the queue of workers so we can be called
  // again:
  CALL(this->image->busy, remove, this->image, (void *)this);
  CALL(this->image->workers, put, (void *)this);
  return result;
};

VIRTUAL(ImageWorker, AFFObject)
     VMETHOD_BASE(ImageWorker, Con) = ImageWorker_Con;
END_VIRTUAL

static AFFObject Image_Con(AFFObject self, char *uri, char mode) {
  Image this=(Image)self;
  char *value;

  if(uri) {
    uint32_t tmp = time(NULL);

    self->urn = talloc_strdup(self, uri);

    // These are the essential properties:
    CALL(oracle, set, URNOF(self), AFF4_TIMESTAMP, &tmp, RESOLVER_DATA_UINT32);

    tmp = 32 * 1024;
    this->chunk_size = *(uint32_t *)resolver_get_with_default
      (oracle, self->urn, AFF4_CHUNK_SIZE, &tmp,
       RESOLVER_DATA_UINT32);

    tmp = 8;
    this->compression = *(uint32_t *)resolver_get_with_default
      (oracle, self->urn, AFF4_COMPRESSION, &tmp,
       RESOLVER_DATA_UINT32);
    
    tmp = 2048;
    this->chunks_in_segment = *(uint32_t *)resolver_get_with_default
      (oracle, self->urn, AFF4_CHUNKS_IN_SEGMENT, &tmp,
       RESOLVER_DATA_UINT32);

    this->bevy_size = this->chunks_in_segment * this->chunk_size;

    tmp = 0;
    this->super.size = *(uint32_t *)resolver_get_with_default
      (oracle, self->urn, AFF4_SIZE, &tmp,
       RESOLVER_DATA_UINT32);

    this->segment_count = 0;
    
    // Build writer workers
    if(mode=='w') {
      int i;
      
      this->workers = CONSTRUCT(Queue, Queue, Con, self);
      this->busy = CONSTRUCT(Queue, Queue, Con, self);
      // Make this many workers
      for(i=0;i<3;i++) {
	ImageWorker w = CONSTRUCT(ImageWorker, ImageWorker, Con, self, this);
	CALL(this->workers, put, (void *)w);
      };

      this->current = CALL(this->workers, get, this);
    };

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

  return self;
};
  
static AFFObject Image_finish(AFFObject self) {
  Image this = (Image)self;

  self->mode = 'w';

  // Make sure the oracle knows we are an image
  CALL(oracle, set, self->urn, AFF4_TYPE, AFF4_IMAGE, RESOLVER_DATA_STRING);

  EVP_DigestInit(&this->digest, EVP_sha256());
  //EVP_DigestInit(&this->digest, EVP_md5());
  return CALL(self, Con, self->urn, 'w');
};

static int Image_write(FileLikeObject self, char *buffer, unsigned long int length) {
  Image this = (Image)self;
  int available_to_read;
  int buffer_readptr=0;

  EVP_DigestUpdate(&this->digest, buffer, length);

  while(length > buffer_readptr) {
    int result;

    available_to_read = min(length - buffer_readptr, this->bevy_size - \
			    this->current->bevy->size);
    if(available_to_read==0) {
      // This bevy is complete - we send it on its way - The worker
      // will be returned to the workers queue when its done.
      this->current->segment_count = this->segment_count;

      // put the worker in the busy queue
      CALL(this->busy, put, this->current);
      pthread_create( &this->current->thread, NULL, 
		      (void *(*) (void *))dump_bevy, (void *)this->current);
      this->segment_count++;

      // Get another worker from the queue:
      this->current = CALL(this->workers, get, self);
      continue;
    };

    result = CALL(this->current->bevy, write, buffer+buffer_readptr, available_to_read);
    buffer_readptr += result;
  };

  self->size += length;
  CALL(oracle, set, URNOF(self), AFF4_SIZE, &self->size, RESOLVER_DATA_UINT64);
  
  return length;
};

static void Image_close(FileLikeObject self) {
  Image this = (Image)self;
  unsigned char buff[BUFF_SIZE];
  unsigned int len;
  unsigned char hash_base64[BUFF_SIZE];

  memset(buff, 0, BUFF_SIZE);

  // Write the last chunk
  this->current->segment_count = this->segment_count;
  dump_bevy(this->current);

  // Wait for all busy threads to finish - its not safe to traverse
  // the busy list without locking it. So we lock it - pull the first
  // item off and join it. Its not a race since the worker is never
  // freed and just moves to the workers queue - even if it happens
  // from under us we should be able to join immediately.
  while(1) {
    Queue item;
    ImageWorker worker;

    pthread_mutex_lock(&this->busy->mutex);
    if(list_empty(&this->busy->list)) {
      pthread_mutex_unlock(&this->busy->mutex);
      break;
    } else {
      list_next(item, &this->busy->list, list);
      pthread_mutex_unlock(&this->busy->mutex);

      worker = (ImageWorker)item->data;
      pthread_join(worker->thread, NULL);
    };
  };

  dump_stream_properties(self);

  {
    TDB_DATA tmp;
    
    tmp.dptr = buff;
    EVP_DigestFinal(&this->digest, buff, &tmp.dsize);

    CALL(oracle, set, URNOF(self), AFF4_SHA, &tmp, RESOLVER_DATA_TDB_DATA);
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
  unsigned long int uncompressed_length=this->chunk_size + 1024;

  /** Now we need to figure out where the segment is */
  char buffer[BUFF_SIZE];
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
  // the segment index which is a segment:
  {
    FileLikeObject fd = (FileLikeObject)CALL(oracle, open, buffer, 'r');
    int32_t *chunk_index;
    int chunks_in_segment;

    if(!fd) {
      ClearError();
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

  fd = (FileLikeObject)CALL(oracle, open, buffer, 'r');

  // Fetch the compressed chunk
  CALL(fd, seek, chunk_offset_in_segment, SEEK_SET);

  //  printf("compressed_length %d %d ", compressed_length, uncompressed_length);
  CALL(fd, read, compressed_chunk, compressed_length);

  // Done with parent and fd
  CALL(oracle, cache_return, (AFFObject)fd);

  if(this->compression == 8) {
    // Try to decompress it:
    if(uncompress((unsigned char *)uncompressed_chunk, 
		  &uncompressed_length, 
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

void dump_stream_properties(FileLikeObject self) {
  char tmp[BUFF_SIZE];
  char *properties;
  char *volume = (char *)CALL(oracle, resolve, self, URNOF(self), AFF4_STORED,
			      RESOLVER_DATA_URN);

  // Set the stream size
  CALL(oracle, set, URNOF(self), AFF4_SIZE, 
       &self->size, RESOLVER_DATA_UINT64);

  // Write out a properties file
  properties = CALL(oracle, export_urn, URNOF(self), URNOF(self));
  if(properties) {
    ZipFile zipfile = (ZipFile)CALL(oracle, open, volume, 'w');

    snprintf(tmp, BUFF_SIZE, "%s/properties", ((AFFObject)self)->urn);
    CALL((ZipFile)zipfile, writestr, tmp, ZSTRING_NO_NULL(properties),
	 NULL, 0, ZIP_STORED);

    talloc_free(properties);
    // Done with zipfile
    CALL(oracle, cache_return, (AFFObject)zipfile);
  };
};


VIRTUAL(Image, FileLikeObject)
     VMETHOD_BASE(AFFObject, Con) = Image_Con;
     VMETHOD_BASE(AFFObject, finish) = Image_finish;

     VMETHOD_BASE(FileLikeObject, read) = Image_read;
     VMETHOD_BASE(FileLikeObject, write) = Image_write;
     VMETHOD_BASE(FileLikeObject,close) = Image_close;
END_VIRTUAL

