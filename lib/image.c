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
#define IMAGE_SEGMENT_NAME_FORMAT "%08d"  /** Stream URN, segment_id */

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

  this->bevy_urn = new_RDFURN(self);

  this->image = image;
  this->bevy = CONSTRUCT(StringIO, StringIO, Con, self);

  this->segment_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  this->index = (IntegerArrayBinary)CALL(oracle, new_rdfvalue,
                                         self, AFF4_INTEGER_ARRAY_BINARY);

  return self;
};

static int dump_bevy_thread(ImageWorker this);

// Compresses all the data in the current bevy and dumps it on the
// storage. The bevy must be as full as can be (image->bevy_size)
// before we dump it (except for the last bevy in the image which can
// be short).
static int dump_bevy(ImageWorker this,  int bevy_number, int backgroud) {
  char buff[BUFF_SIZE];
  RDFURN stream_urn = URNOF(this->image);

  snprintf(buff, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, bevy_number);

  // We prepare the bevy's URN accordingly
  CALL(URNOF(this), set, stream_urn->value);
  CALL(URNOF(this), add, buff);

  // Make the bevy get stored in the same place the image is:
  CALL(oracle, set_value, URNOF(this),
       AFF4_VOLATILE_STORED, (RDFValue)this->image->stored);

  if(backgroud) {
    // put the worker in the busy queue
    CALL(this->image->busy, put, this);

    // Invoke a thread to do the work
    pthread_create( &this->thread, NULL,
                    (void *(*) (void *))dump_bevy_thread, (void *)this);


    // Get another worker from the queue:
    this->image->current = CALL(this->image->workers, get, this->image);
  } else {
    dump_bevy_thread(this);
  };

  return 0;
};

static int dump_bevy_thread(ImageWorker this) {
  int bevy_index=0;
  int result;
  FileLikeObject bevy;

  AFF4_LOG(AFF4_LOG_MESSAGE, "Dumping bevy %s (%d bytes)\n",
           URNOF(this)->value, this->bevy->size);

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
    CALL(this->index, add, this->segment_buffer->readptr);

    // Now write the buffer to the stream buffer
    CALL(this->segment_buffer, write, cbuffer, clength);
    bevy_index += length;
  };

  bevy = (FileLikeObject)CALL(oracle, create, AFF4_SEGMENT, 'w');
  CALL(URNOF(bevy), set, URNOF(this)->value);

  bevy = (FileLikeObject)CALL((AFFObject)bevy, finish);
  if(!bevy) {
    result = -1;
    goto error;
  };

  CALL(bevy, write, this->segment_buffer->data,
       this->segment_buffer->readptr);

  CALL(bevy, close);

  // If the index is small enough, make it inline
  if(this->index->size < 2) {
    ((RDFValue)this->index)->dataType = ((RDFValue)&__IntegerArrayInline)->dataType;
    ((RDFValue)this->index)->serialise = ((RDFValue)&__IntegerArrayInline)->serialise;
    ((RDFValue)this->index)->id = ((RDFValue)&__IntegerArrayInline)->id;
  };

  // Set the index on the bevy
  CALL(oracle, set_value, URNOF(bevy), AFF4_INDEX, (RDFValue)this->index);

  // Reset everything to the start
  CALL(this->segment_buffer, truncate, 0);
  CALL(this->bevy, truncate, 0);

  // Make a new index
  talloc_free(this->index);
  this->index = (IntegerArrayBinary)CALL(oracle, new_rdfvalue,
                                         this, AFF4_INTEGER_ARRAY_BINARY);

  result = 1;

 error:
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
      // will be returned to the workers queue when its done. Note
      // that the worker has the same URN of the bevy it will produce.

      // Invoke a thread to do the work
      dump_bevy(this->current, this->segment_count, 1);
      this->segment_count++;
      continue;
    };

    result = CALL(this->current->bevy, write, buffer+buffer_readptr, available_to_read);
    buffer_readptr += result;
  };

  self->readptr += length;
  self->size->value = max(self->size->value, self->readptr);
  CALL(oracle, set_value, URNOF(self), AFF4_SIZE,
       (RDFValue)((FileLikeObject)self)->size);

  return length;
};

static void Image_close(FileLikeObject self) {
  Image this = (Image)self;

  // Write the last chunk
  this->current->segment_count = this->segment_count;
  dump_bevy(this->current, this->segment_count, 0);

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

  // Update the size
  CALL(oracle, set_value, URNOF(self), AFF4_VOLATILE_SIZE,
       (RDFValue)self->size);

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
  IntegerArrayBinary index;

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

  // Form the bevy name
  {
    char buff[BUFF_SIZE];

    snprintf(buff, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, segment_id);
    CALL(this->bevy_urn, set, URNOF(self)->value);
    CALL(this->bevy_urn, add, buff);
  };

  // Try to resolve it
  index = (IntegerArrayBinary)CALL(oracle, resolve_alloc,
                                   chunk_cache, this->bevy_urn, AFF4_INDEX);
  if(!index) {
    RaiseError(ERuntimeError, "No index for bevy %s?", this->bevy_urn->value);
    goto error;
  };

  // The last index point is maxint - this makes the following
  // calculations easier
  CALL(index, add, 0xFFFFFFFF);

  if(chunk_index_in_segment + 1 > index->size) {
    RaiseError(ERuntimeError, "Index is too small");
    goto error;
  };

  compressed_length = min((uint32_t)index->array[chunk_index_in_segment+1] -
                          (uint32_t)index->array[chunk_index_in_segment],
                          this->chunk_size->value);

  chunk_offset_in_segment = index->array[chunk_index_in_segment];

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
  available_to_read = min(available_to_read, chunk_cache->len);

  // Copy it on the result stream
  memcpy(buffer, chunk_cache->data + chunk_offset, available_to_read);
  self->readptr += available_to_read;

  // Cache the chunk (This ensures it goes on the head of the
  // list):
  CALL(this->chunk_cache, put, (char *)&chunk_id, sizeof(chunk_id),
       (Object)chunk_cache);

  // The cache will now own this:
  talloc_unlink(NULL, chunk_cache);

  // Non error path
  ClearError();

  return available_to_read;

  // Pad the buffer on error:
  error: {
    memset(buffer,0, length);

    self->readptr += available_to_read;
    if(chunk_cache)
      talloc_unlink(NULL, chunk_cache);
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
