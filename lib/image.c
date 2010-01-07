#include "aff4.h"
#include "exports.h"
#include "encode.h"

/*************************************************************
  The Image stream works by collecting chunks into segments. Chunks
  are compressed seperately using zlib's compress function.

  Defined attributes:

  aff4:type                "image"
  aff4:chunk_size          The size of the chunk in bytes (32k)
  aff4:chunks_in_segment   The number of chunks in each segment (2048)
  aff4:stored              The URN of the object which stores this
                           stream - This must be a "volume" object
  aff4:size                The size of this stream in bytes (0)

  Note that bevies are segment objects with an implied URN of:

  "%s/%08d" % (Image.urn, bevy_number)


This implementation uses threads to compress bevies concurrently. We
also maintain a chunk cache for faster read access.

**************************************************************/

/** This is a chunck of data we cache */
CLASS(ChunkCache, Object)
     char *data;
     int len;

     ChunkCache METHOD(ChunkCache, Con, int len);
END_CLASS

ChunkCache ChunkCache_Con(ChunkCache self, int len) {
  self->len = len;
  self->data = talloc_size(self, len);

  return self;
};

VIRTUAL(ChunkCache, Object) {
  VMETHOD(Con) = ChunkCache_Con;
} END_VIRTUAL;

// The segments created by the Image stream are named by segment
// number with this format.
#define IMAGE_SEGMENT_NAME_FORMAT "%s/%08d"  /** Stream URN, segment_id */

/** This specialised hashing is for integer keys */
static unsigned int cache_hash_int(Cache self, char *key, int len) {
  uint32_t int_key = *(uint32_t *)key;

  return int_key % self->hash_table_width;
};

static int cache_cmp_int(Cache self, char *other, int len) {
  uint32_t int_key = *(uint32_t *)self->key;
  uint32_t int_other = *(uint32_t *)other;

  return int_key != int_other;
};

static ImageWorker ImageWorker_Con(ImageWorker self, Image image) {
  ImageWorker this=self;

  URNOF(self) = CALL(URNOF(image), copy, self);
  
  this->stored = new_RDFURN(self);

  this->image = image;
  this->bevy = CONSTRUCT(StringIO, StringIO, Con, self);
  
  this->segment_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  this->chunk_indexes = talloc_array(self, int32_t, 
				     image->chunks_in_segment->value + 2);

  // Fill it with -1 to indicate an invalid pointer
  memset(this->chunk_indexes, 0xff, 
	 sizeof(int32_t) * image->chunks_in_segment->value);

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
    int length = min(this->image->chunk_size->value, this->bevy->size - bevy_index);
    // We just use compress() to get the compressed buffer.
    char cbuffer[2*compressBound(this->image->chunk_size->value)];
    int clength=2*compressBound(this->image->chunk_size->value);

    // Should we offer to store chunks uncompressed?
    if(this->image->compression->value == 0) {
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
    char buff[BUFF_SIZE];
    AFF4Volume parent;

    if(!CALL(oracle, resolve_value, URNOF(this), AFF4_STORED, (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "No storage for Image stream?");
      goto error;
    }

    parent  = (AFF4Volume)CALL(oracle, open, this->stored, 'w');

    // Format the segment name
    snprintf(buff, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, 
	     ((AFFObject)this)->urn->value, 
	     segment_number);
    
    // Push one more offset to the index to cover the last chunk
    this->chunk_indexes[chunk_count + 1] = this->segment_buffer->readptr;

    AFF4_LOG(AFF4_LOG_MESSAGE, "Dumping bevy %s (%lld bytes)\n", 
             buff, this->segment_buffer->readptr);

    // Store the entire segment in the zip file
    CALL((AFF4Volume)parent, writestr,
	 buff, this->segment_buffer->data, 
	 this->segment_buffer->readptr, 
	 // No compression for these segments
	 ZIP_STORED
	 );

    // Now write the index file which accompanies the segment
    snprintf(buff, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT ".idx", 
	     ((AFFObject)this)->urn->value, 
	     segment_number);

    CALL((AFF4Volume)parent, writestr,
	 buff, (char *)this->chunk_indexes,
	 (chunk_count + 1) * sizeof(uint32_t),
	 ZIP_STORED
	 );

    // Done with parent
    CALL(oracle, cache_return, (AFFObject)parent);

    // Reset everything to the start
    CALL(this->segment_buffer, truncate, 0);
    CALL(this->bevy, truncate, 0);
    memset(this->chunk_indexes, -1,
	   sizeof(int32_t) * this->image->chunks_in_segment->value);
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

VIRTUAL(ImageWorker, AFFObject) {
  VMETHOD_BASE(ImageWorker, Con) = ImageWorker_Con;
} END_VIRTUAL

static AFFObject Image_Con(AFFObject self, RDFURN uri, char mode) {
  Image this=(Image)self;

  // Call our baseclass
  self = SUPER(AFFObject, FileLikeObject, Con, uri, mode);

  if(self && uri) {
    URNOF(self) = CALL(uri, copy, self);
    this->chunk_size = new_XSDInteger(self);
    this->compression = new_XSDInteger(self);
    this->chunks_in_segment = new_XSDInteger(self);
    this->stored = new_RDFURN(self);
    this->bevy_urn = new_RDFURN(self);

    // Some defaults
    this->chunk_size->value = 32*1024;
    this->compression->value = ZIP_DEFLATE;
    this->chunks_in_segment->value = 2048;

    if(!CALL(oracle, resolve_value, uri, AFF4_STORED, (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "Image not stored anywhere?");
      goto error;
    };

    // Add ourselves to our volume
    CALL(oracle, add_value, this->stored, AFF4_VOLATILE_CONTAINS, (RDFValue)URNOF(self));

    // These are the essential properties:
    //CALL(oracle, set, URNOF(self), AFF4_TIMESTAMP, &tmp, RESOLVER_DATA_UINT32);


    CALL(oracle, resolve_value, URNOF(self), AFF4_CHUNK_SIZE,
	 (RDFValue)this->chunk_size);

    CALL(oracle, resolve_value, URNOF(self), AFF4_COMPRESSION,
	 (RDFValue)this->compression);

    CALL(oracle, resolve_value, URNOF(self), AFF4_CHUNKS_IN_SEGMENT,
	 (RDFValue)this->chunks_in_segment);

    CALL(oracle, resolve_value, URNOF(self), AFF4_SIZE,
	 (RDFValue)((FileLikeObject)this)->size);

    this->bevy_size = this->chunks_in_segment->value * this->chunk_size->value;
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
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};
  
static int Image_write(FileLikeObject self, char *buffer, unsigned long int length) {
  Image this = (Image)self;
  int available_to_read;
  int buffer_readptr=0;

  //  EVP_DigestUpdate(&this->digest, buffer, length);

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

      // Just call it in place for now (no multithreading right now).
      dump_bevy(this->current);
      //      pthread_create( &this->current->thread, NULL, 
      //		      (void *(*) (void *))dump_bevy, (void *)this->current);
      this->segment_count++;

      // Get another worker from the queue:
      this->current = CALL(this->workers, get, self);
      continue;
    };

    result = CALL(this->current->bevy, write, buffer+buffer_readptr, available_to_read);
    buffer_readptr += result;
  };

  self->size->value += length;
  CALL(oracle, set_value, URNOF(self), AFF4_SIZE,
       (RDFValue)((FileLikeObject)self)->size);
  
  return length;
};

static void Image_close(FileLikeObject self) {
  Image this = (Image)self;

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

  // Now store all our parameters in the resolver
  CALL(oracle, set_value, URNOF(this), AFF4_CHUNK_SIZE,
       (RDFValue)this->chunk_size);

  CALL(oracle, set_value, URNOF(this), AFF4_COMPRESSION,
       (RDFValue)this->compression);

  CALL(oracle, set_value, URNOF(this), AFF4_CHUNKS_IN_SEGMENT,
       (RDFValue)this->chunks_in_segment);

  CALL(oracle, set_value, URNOF(this), AFF4_TYPE,
       rdfvalue_from_string(this, AFF4_IMAGE));

  {
    XSDDatetime time = new_XSDDateTime(this);

    gettimeofday(&time->value,NULL);
    CALL(oracle, set_value, URNOF(this), AFF4_TIMESTAMP,
	 (RDFValue)time);
  };

  // FIXME - implement sha256 RDF dataType
#if 0
  {
    TDB_DATA tmp;
    
    tmp.dptr = buff;
    EVP_DigestFinal(&this->digest, buff, &tmp.dsize);

    CALL(oracle, set_value, URNOF(self), AFF4_SHA, &tmp, RESOLVER_DATA_TDB_DATA);
  };
#endif

  SUPER(FileLikeObject, FileLikeObject, close);
};

/** Reads at most a single chunk and write to result. Return how much
    data was actually read.
*/
static int partial_read(FileLikeObject self, char *buffer, int length) {
  Image this = (Image)self;

  // which segment is it?
  uint32_t chunk_id = self->readptr / this->chunk_size->value;
  int segment_id = chunk_id / this->chunks_in_segment->value;
  int chunk_index_in_segment = chunk_id % this->chunks_in_segment->value;
  int chunk_offset = self->readptr % this->chunk_size->value;
  int available_to_read = min(this->chunk_size->value - chunk_offset, length);

  /* Temporary storage for the uncompressed chunk */
  unsigned long int compressed_length;

  /** Now we need to figure out where the segment is */
  FileLikeObject fd;
  int32_t chunk_offset_in_segment;

  /** Fast path - check if the chunk is already cached.  If it is - we
  can just copy a subset of it on the result and get out of here....
  Note that we maintain a seperate cache of chunks because if we
  cached the entire segment (which could be very large) we would
  exhaust our cache memory very quickly.
  */
  ChunkCache chunk_cache = (ChunkCache)CALL(this->chunk_cache, get, 
                                            (char *)&chunk_id, sizeof(chunk_id));

  if(chunk_cache) {
    goto exit;
  };

  /** Cache miss... */
  chunk_cache = CONSTRUCT(ChunkCache, ChunkCache, Con, NULL,
                          this->chunk_size->value);

  /** First we need to locate the chunk indexes */
  snprintf(buffer, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT ".idx",
	   ((AFFObject)this)->urn->value,
	   segment_id);
  CALL(this->bevy_urn, set, buffer);

  // Now we work out the offsets on the chunk in the segment - we read
  // the segment index which is a segment:
  {
    FileLikeObject fd = (FileLikeObject)CALL(oracle, open, this->bevy_urn, 'r');
    int32_t *chunk_index;
    int chunks_in_segment;

    if(!fd) {
      ClearError();
      RaiseError(ERuntimeError, "Unable to locate index %s", buffer);
      goto error;
    };

    // FIXME - this needs to be more efficient with seeking and
    // reading the required index from the chunk index rather than
    // reading the entire index into memory at once.

    /** This holds the index into the segment of all the chunks.
	It is an array of chunks_in_segment ints long.
    */
    chunk_index = (int32_t *)CALL(fd, get_data);
    chunks_in_segment = fd->size->value / sizeof(int32_t);

    /** By here we have the chunk_index which is an array of offsets
	into the segment where the chunks begin. We work out the size of
	the chunk by subtracting the next chunk offset from the current
	one. It seems to be ok to over read here so if the numbers dont
	make too much sense we just choose to overread.
    */
    compressed_length = min((uint32_t)chunk_index[chunk_index_in_segment+1] -
			    (uint32_t)chunk_index[chunk_index_in_segment],
			    this->chunk_size->value);

    chunk_offset_in_segment = (uint32_t)chunk_index[chunk_index_in_segment];

    // Done with the chunk index
    CALL(oracle, cache_return, (AFFObject)fd);
  };

  /** Now obtain a handle directly into the segment */
  snprintf(buffer, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT,
           ((AFFObject)this)->urn->value,
           segment_id);

  CALL(this->bevy_urn, set, buffer);

  // Now open the bevy
  fd = (FileLikeObject)CALL(oracle, open, this->bevy_urn, 'r');

  // Fetch the compressed chunk
  CALL(fd, seek, chunk_offset_in_segment, SEEK_SET);

  switch(this->compression->value) {
  case ZIP_DEFLATE: {
    /* Temporary storage for the compressed chunk */
    char compressed_chunk[compressed_length];

    CALL(fd, read, compressed_chunk, compressed_length);

    // Try to decompress it:
    if(uncompress((unsigned char *)chunk_cache->data,
		  (unsigned long int *)&chunk_cache->len,
		  (unsigned char *)compressed_chunk,
		  (unsigned long int)compressed_length) != Z_OK ) {
      RaiseError(ERuntimeError, "Unable to decompress chunk %d", chunk_id);
      goto error;
    };

    break;
  };
    /** No compression */
  case ZIP_STORED:
    CALL(fd, read, chunk_cache->data, chunk_cache->len);
    break;

  default:
    RaiseError(ERuntimeError, "Unknown bevy compression of %d", this->compression->value);
    goto error;

  };

  // Done with fd
  CALL(oracle, cache_return, (AFFObject)fd);

 exit:
  // Now copy the data out of the chunk_cache
  available_to_read = min(length, chunk_cache->len);

  // Copy it on the result stream
  memcpy(buffer, chunk_cache->data + chunk_offset, available_to_read);
  self->readptr += available_to_read;

  // Cache the chunk (This ensures it goes on the head of the
  // list):
  CALL(this->chunk_cache, put, (char *)&chunk_id, sizeof(chunk_id),
       (Object)chunk_cache);

  // The cache will now own this:
  talloc_free(chunk_cache);

  // Non error path
  ClearError();

  return available_to_read;

  // Pad the buffer on error:
  error: {
    memset(buffer,0, length);

    self->readptr += available_to_read;
    if(chunk_cache)
      talloc_free(chunk_cache);
    return length;
  };
};

// Reads from the image stream
static int Image_read(FileLikeObject self, char *buffer, unsigned long int length) {
  int read_length=0;
  int available_to_read = min(length, self->size->value - self->readptr);

  // Just execute as many partial reads as are needed to satisfy the
  // length requested
  while(read_length < available_to_read ) {
    int res = partial_read(self, buffer + read_length, available_to_read - read_length);

    if(res <=0) break;

    read_length += res;
  };

  return read_length;
};

VIRTUAL(Image, FileLikeObject) {
     VMETHOD_BASE(AFFObject, Con) = Image_Con;
     VMETHOD_BASE(AFFObject, dataType) = AFF4_IMAGE;

     VMETHOD_BASE(FileLikeObject, read) = Image_read;
     VMETHOD_BASE(FileLikeObject, write) = Image_write;
     VMETHOD_BASE(FileLikeObject,close) = Image_close;
} END_VIRTUAL

void image_init() {
  INIT_CLASS(Image);

  register_type_dispatcher(AFF4_IMAGE, (AFFObject *)GETCLASS(Image));

};
