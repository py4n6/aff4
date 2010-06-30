#include "misc.h"

#define MAX_IMAGER_THREADS 3

/** This class is used by the image worker thread to dump the segments
    out. It is only created by the Image class internally.
*/
PRIVATE CLASS(ImageWorker, AFFObject)
       struct list_head list;

  // The URL of the bevy we are working on now:
       RDFURN bevy_urn;

       int segment_count;

       // This is the queue which this worker belongs in
       Queue queue;

       // The bevy is written here in its entirety. When its finished,
       // we compress it all and dump it to the output file.
       StringIO bevy;

       // The segment is written here until it is complete and then it
       // gets flushed
       StringIO segment_buffer;

       // When we finish writing a bevy, a thread is launched to
       // compress and dump it
       pthread_t thread;
       struct Image_t *image;

       // The index into the bevy - we use an IntegerArrayBinary
       // RDFValue, unless the array is very small in which case we
       // switch to the IntegerArrayInline
       IntegerArrayBinary index;

       ImageWorker METHOD(ImageWorker, Con, struct Image_t *image);

       // A write method for the worker
       int METHOD(ImageWorker, write, char *buffer, int len);
END_CLASS


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
     Bytef *data;
     uLongf len;

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

static void join_worker_threads(Queue workers) {
  // Make sure all the threads joined
  if(workers){
    Queue i;

    pthread_mutex_lock(&workers->mutex);
    list_for_each_entry(i, &workers->list, list) {
        ImageWorker w = (ImageWorker)i->data;

        if(w->thread ) {
          void *foo;
          pthread_join( w->thread, &foo );
          w->thread = 0;
        };
      };
    pthread_mutex_unlock(&workers->mutex);
  };
};

static int dump_bevy_thread(ImageWorker this);

/* Compresses all the data in the current bevy and dumps it on the
   storage. The bevy must be as full as can be (image->bevy_size)
   before we dump it (except for the last bevy in the image which can
   be short).

   We return the length of the bevy is successful, 0 on error.
*/
static int dump_bevy(ImageWorker this,  int bevy_number, int backgroud) {
  char buff[BUFF_SIZE];
  RDFURN stream_urn = URNOF(this->image);

  snprintf(buff, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, bevy_number);

  // We prepare the bevy's URN accordingly
  CALL(URNOF(this), set, stream_urn->value);
  CALL(URNOF(this), add, buff);

  // Make the bevy get stored in the same place the image is:
  CALL(oracle, set_value, URNOF(this),
       AFF4_VOLATILE_STORED, (RDFValue)this->image->stored,0);

  /* We are running in multi-threaded mode */
  if(this->image->busy && backgroud) {
    // put the worker in the busy queue
    CALL(this->image->busy, put, this);

    // Invoke a thread to do the work
    pthread_create( &this->thread, NULL,
                    (void *(*) (void *))dump_bevy_thread, (void *)this);

    // Get another worker from the queue:
    this->image->current = CALL(this->image->workers, get, this->image);

    if(this->image->current->thread) {
      void *foo;
      pthread_join( this->image->current->thread, &foo);
      this->image->current->thread = 0;
    };

    join_worker_threads(this->image->workers);

    /* FIXME: We have no way of passing back if the worker had any
       errors. what should we do?
    */
    return 1;
  } else {
    return dump_bevy_thread(this);
  };

  return 0;
};

static int dump_bevy_thread(ImageWorker this) {
  int bevy_index=0;
  int result=0;
  FileLikeObject bevy;
  AFF4Volume volume;
  RDFURN bevy_urn=NULL;

  while(bevy_index < this->bevy->size) {
    uLong length = min(this->image->chunk_size->value, this->bevy->size - bevy_index);
    // We just use compress() to get the compressed buffer.
    char cbuffer[2*compressBound(this->image->chunk_size->value)];
    uLongf clength=2*compressBound(this->image->chunk_size->value);

    // Should we offer to store chunks uncompressed?
    if(this->image->compression->value == 0) {
      memcpy(cbuffer, this->bevy->data + bevy_index, length);
      clength = length;
    } else {
      if(compress2((Bytef *)cbuffer, &clength,
		   (Bytef *)this->bevy->data + bevy_index,
                   (uLong)length, 1) != Z_OK) {
	RaiseError(ERuntimeError, "Compression error");
        return 0;
      };
    };

    // Update the index to point at the current segment stream buffer
    // offset
    CALL(this->index, add, this->segment_buffer->readptr);

    // Now write the buffer to the stream buffer
    CALL(this->segment_buffer, write, cbuffer, clength);
    bevy_index += length;
  };

  // Get the latest word on where this image is stored - this allows
  // the user to switch the image container at any time.
  if(!CALL(oracle, resolve_value, URNOF(this->image),
           AFF4_STORED, (RDFValue)this->image->stored)) {
    RaiseError(ERuntimeError, "Image %s is not stored anywhere?", STRING_URNOF(this->image));
    goto error;
  };

  volume = (AFF4Volume)CALL(oracle, open, this->image->stored, 'w');
  if(!volume) goto error;

  bevy = (FileLikeObject)CALL(volume, open_member, URNOF(this)->value,
                              'w', ZIP_STORED);

  // We are done with the volume here
  CALL((AFFObject)volume, cache_return);

  if(!bevy) {
    result = -1;
    goto error;
  };

  // Keep a copy of the URN around for later
  bevy_urn = URNOF(bevy);
  talloc_reference(this, bevy_urn);

  CALL(bevy, write, this->segment_buffer->data,
       this->segment_buffer->readptr);

  /* We need to release the file as soon as possible because later
     serializing might need to write its own segments.
  */
  CALL((AFFObject)bevy, close);

  // Stupid divide by zero ....
  if(this->bevy->size > 0) {
    AFF4_LOG(AFF4_LOG_MESSAGE, AFF4_SERVICE_IMAGE_STREAM,
             URNOF(this),
             "Bevy Dumped ( %dkbytes %d%%)\n",
             this->segment_buffer->size/1024,
             this->segment_buffer->size * 100 /this->bevy->size);
  };

  // Now dump the index by serializing the RDFValue
  // If the index is small enough, make it inline
  if(this->index->size < MAX_SIZE_OF_INLINE_ARRAY) {
    ((RDFValue)this->index)->dataType = ((RDFValue)&__IntegerArrayInline)->dataType;
    ((RDFValue)this->index)->serialise = ((RDFValue)&__IntegerArrayInline)->serialise;
    ((RDFValue)this->index)->id = ((RDFValue)&__IntegerArrayInline)->id;
  };

  // Set the index on the bevy - trap any possible errors
  if(!CALL(oracle, set_value, bevy_urn, AFF4_INDEX, (RDFValue)this->index,0)) {
    goto error;
  };

#if 1
  // Reset everything to the start
  talloc_free(this->segment_buffer);
  talloc_free(this->bevy);

  this->segment_buffer = CONSTRUCT(StringIO, StringIO, Con, this);
  this->bevy = CONSTRUCT(StringIO, StringIO, Con, this);
#else
  CALL(this->bevy, truncate, 0);
  CALL(this->segment_buffer, truncate, 0);
#endif

  // Make a new index
  talloc_free(this->index);
  this->index = (IntegerArrayBinary)CALL(oracle, new_rdfvalue,
                                         this, AFF4_INTEGER_ARRAY_BINARY);

  result = 1;

  // Fall through to put ourselves back on the busy queue.

 error:
  // Return ourselves to the queue of workers so we can be called
  // again:
  if(this->image->busy) {
    CALL(this->image->busy, remove, NULL, (Object)this);
    CALL(this->image->workers, put, (Object)this);
  };

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
    this->chunk_size = new_XSDInteger(self);
    this->compression = new_XSDInteger(self);
    this->chunks_in_segment = new_XSDInteger(self);
    this->stored = new_RDFURN(self);
    this->bevy_urn = new_RDFURN(self);

    // Some defaults
    this->chunk_size->value = 32*1024;
    this->compression->value = ZIP_DEFLATE;
    this->chunks_in_segment->value = 500;

    if(!CALL(oracle, resolve_value, uri, AFF4_STORED, (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "Image not stored anywhere?");
      goto error;
    } else {
      // Make sure we can actually open the containing volume
      AFF4Volume volume = (AFF4Volume)CALL(oracle, open, this->stored, mode);
      if(!volume) goto error;
      CALL((AFFObject)volume, cache_return);
    };

    // Add ourselves to our volume
    CALL(oracle, add_value, this->stored, AFF4_VOLATILE_CONTAINS,
         (RDFValue)URNOF(self),0);

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
      this->current = CONSTRUCT(ImageWorker, ImageWorker, Con, this, this);
      CALL(oracle, set_value, URNOF(this), AFF4_TYPE,
           rdfvalue_from_urn(this, AFF4_IMAGE),0);
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
  talloc_unlink(NULL, self);
  return NULL;
};


static void Image_set_workers(Image this, int workers) {
  int i;

  if(workers>0) {
    this->workers = CONSTRUCT(Queue, Queue, Con, this);
    this->busy = CONSTRUCT(Queue, Queue, Con, this);
    // Make this many workers
    for(i=0;i < workers; i++) {
      ImageWorker w = CONSTRUCT(ImageWorker, ImageWorker, Con, this, this);
      CALL(this->workers, put, (void *)w);
    };
  };
};

static int Image_write(FileLikeObject self, char *buffer, unsigned long int length) {
  Image this = (Image)self;
  int available_to_read;
  int buffer_readptr=0;

  //  EVP_DigestUpdate(&this->digest, buffer, length);

  while(length > buffer_readptr) {
    int result;

    available_to_read = min(length - buffer_readptr, this->bevy_size - \
			    this->current->bevy->readptr);
    if(available_to_read==0) {
      // This bevy is complete - we send it on its way - The worker
      // will be returned to the workers queue when its done. Note
      // that the worker has the same URN of the bevy it will produce.

      // Invoke a thread to do the work
      if(!dump_bevy(this->current, this->segment_count, 1)) {
        this->segment_count++;
        goto error;
      };
      /*
      //Preallocate the memory in the worker's buffer to save on memcpys
      CALL(this->current->bevy, seek, this->bevy_size, SEEK_SET);
      CALL(this->current->bevy, truncate, 0);
      CALL(this->current->segment_buffer, seek, this->bevy_size, SEEK_SET);
      CALL(this->current->segment_buffer, truncate, 0);
      */

      this->segment_count++;
      continue;
    };

    result = CALL(this->current->bevy, write, buffer+buffer_readptr, available_to_read);
    buffer_readptr += result;
  };

  self->readptr += length;
  self->size->value = max(self->size->value, self->readptr);
  CALL(oracle, set_value, URNOF(self), AFF4_SIZE,
       (RDFValue)((FileLikeObject)self)->size,0);

  return length;

 error:
  return -1;
};

static int Image_close(AFFObject aself) {
  FileLikeObject self = (FileLikeObject)aself;
  Image this = (Image)self;

  // We only do stuff when opened for writing
  if(((AFFObject)self)->mode != 'w') goto exit;

  // Write the last chunk
  this->current->segment_count = this->segment_count;
  if(!dump_bevy(this->current, this->segment_count, 0))
    goto exit;

  // Wait for all busy threads to finish - its not safe to traverse
  // the busy list without locking it. So we lock it - pull the first
  // item off and join it. Its not a race since the worker is never
  // freed and just moves to the workers queue - even if it happens
  // from under us we should be able to join immediately.
  while(this->busy) {
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

  join_worker_threads(this->workers);

  // Now store all our parameters in the resolver
  CALL(oracle, set_value, URNOF(this), AFF4_CHUNK_SIZE,
       (RDFValue)this->chunk_size,0);

  CALL(oracle, set_value, URNOF(this), AFF4_COMPRESSION,
       (RDFValue)this->compression,0);

  CALL(oracle, set_value, URNOF(this), AFF4_CHUNKS_IN_SEGMENT,
       (RDFValue)this->chunks_in_segment,0);

  {
    XSDDatetime time = new_XSDDateTime(this);

    gettimeofday(&time->value,NULL);
    CALL(oracle, set_value, URNOF(this), AFF4_TIMESTAMP,
	 (RDFValue)time,0);
  };

  // FIXME - implement sha256 RDF dataType
#if 0
  {
    TDB_DATA tmp;

    tmp.dptr = buff;
    EVP_DigestFinal(&this->digest, buff, &tmp.dsize);

    CALL(oracle, set_value, URNOF(self), AFF4_SHA,
         &tmp, RESOLVER_DATA_TDB_DATA,0);
  };
#endif

  // Update the size
  CALL(oracle, set_value, URNOF(self), AFF4_SIZE,
       (RDFValue)self->size,0);

 exit:
  return SUPER(AFFObject, FileLikeObject, close);
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
                          this->chunk_size->value * 2);

  chunk_offset_in_segment = index->array[chunk_index_in_segment];

  // Now open the bevy
  fd = (FileLikeObject)CALL(oracle, open, this->bevy_urn, 'r');

  // Fetch the compressed chunk
  CALL(fd, seek, chunk_offset_in_segment, SEEK_SET);

  switch(this->compression->value) {
  case ZIP_DEFLATE: {
    /* Temporary storage for the compressed chunk */
    const Bytef compressed_chunk[compressed_length];

    if(CALL(fd, read, (char *)compressed_chunk, compressed_length)<0)
      goto error;

    // Try to decompress it:
    if(uncompress(chunk_cache->data,
		  &chunk_cache->len,
                  compressed_chunk,
		  (uLong)compressed_length) != Z_OK ) {
      RaiseError(ERuntimeError, "Unable to decompress chunk %d", chunk_id);
      goto error;
    };

    break;
  };
    /** No compression */
  case ZIP_STORED:
    CALL(fd, read, (char *)chunk_cache->data, chunk_cache->len);
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

 error:
  if(chunk_cache)
    talloc_unlink(NULL, chunk_cache);
  return -1;
};

// Reads from the image stream
static int Image_read(FileLikeObject self, char *buffer, unsigned long int length) {
  int read_length=0;
  int available_to_read = min(length, self->size->value - self->readptr);

  // Just execute as many partial reads as are needed to satisfy the
  // length requested
  while(read_length < available_to_read ) {
    int res = partial_read(self, buffer + read_length, available_to_read - read_length);

    if(res < 0) return -1;
    if(res==0) break;

    read_length += res;
  };

  return read_length;
};

VIRTUAL(Image, FileLikeObject) {
     VMETHOD_BASE(AFFObject, Con) = Image_Con;
     VMETHOD_BASE(AFFObject, dataType) = AFF4_IMAGE;

     VMETHOD_BASE(FileLikeObject, read) = Image_read;
     VMETHOD_BASE(FileLikeObject, write) = Image_write;
     VMETHOD_BASE(AFFObject, close) = Image_close;

     VMETHOD(set_workers) = Image_set_workers;
} END_VIRTUAL

AFF4_MODULE_INIT(A000_image) {
  INIT_CLASS(ChunkCache);
  INIT_CLASS(ImageWorker);

  register_type_dispatcher(oracle, AFF4_IMAGE, (AFFObject *)GETCLASS(Image));

};
