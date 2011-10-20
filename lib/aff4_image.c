/** This file implements the basic image handling code.
*/
#include "aff4_internal.h"

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

/** This class is used by the image worker thread to dump the segments
    out. It is only created by the Image class internally.
*/
PRIVATE CLASS(ImageWorker, ThreadPoolJob)
     // The AFF4Image object which owns us.
     AFF4Image image;
     int segment_count;

     // The bevy is written here in its entirety. When its finished,
     // we compress it all and dump it to the output file.
     StringIO bevy;

     ImageWorker METHOD(ImageWorker, Con, AFF4Image parent, int segment_count);

     // A write method for the worker
     int METHOD(ImageWorker, close);
END_CLASS


static ImageWorker ImageWorker_Con(ImageWorker self, AFF4Image parent, int segment_count) {
  self->image = parent;
  self->segment_count = segment_count;

  self->bevy = CONSTRUCT(StringIO, StringIO, Con, self);

  return self;
};


static void ImageWorker_run(ThreadPoolJob this) {
  ImageWorker self = (ImageWorker) this;
  RDFURN bevy_urn = CALL(URNOF(self->image), copy, self);
  ZipFile zip;
  FileLikeObject segment, index_segment;
  Resolver resolver = ((AFFObject)(self->image))->resolver;
  uint32_t chunk_offset = 0;
  uint32_t compressed_offset = 0;

  CALL(bevy_urn, add, talloc_asprintf(bevy_urn, "%08X", self->segment_count));

  /* First open the segment so we can write on it. */
  zip = (ZipFile)CALL(resolver, own, self->image->stored, 'w');
  segment = (FileLikeObject)CALL((AFF4Volume)zip, open_member, bevy_urn, 'w', ZIP_STORED);

  CALL(bevy_urn, add, "idx");
  index_segment = (FileLikeObject)CALL((AFF4Volume)zip, open_member, bevy_urn, 'w', ZIP_STORED);

  CALL(resolver, cache_return, (AFFObject)zip);

  /* Now we compress chunks from our bevy into the segment. */
  while(chunk_offset < self->bevy->size) {
    uLong length = min(self->image->chunk_size, self->bevy->size - chunk_offset);

    // We just use compress() to get the compressed buffer.
    uLongf clength = compressBound(self->image->chunk_size);
    char cbuffer[clength];
    char buffer[length];

    memcpy(buffer, self->bevy->data + chunk_offset, length);

    // Should we offer to store chunks uncompressed?
    if(self->image->compression == ZIP_STORED) {
      memcpy(cbuffer, buffer, length);
      clength = length;
    } else {
      int res;

      /* This can run concurrently. */
      AFF4_BEGIN_ALLOW_THREADS;

      res = compress2((Bytef *)cbuffer, &clength,
                      (Bytef *)buffer, (uLong)length, 1);

      AFF4_END_ALLOW_THREADS;

      if(res != Z_OK) {
	RaiseError(ERuntimeError, "Compression error");
        goto error;
      };
    };

    // Update the index to point at the current segment stream buffer
    // offset
    CALL(index_segment, write, (char *)&compressed_offset, sizeof(uint32_t));
    CALL(segment, write, cbuffer, clength);

    compressed_offset += clength;
    chunk_offset += length;
  };

  CALL((AFFObject)segment, close);
  CALL((AFFObject)index_segment, close);

 error:
  return;
};



VIRTUAL(ImageWorker, ThreadPoolJob) {
  VMETHOD(Con) = ImageWorker_Con;
  VMETHOD_BASE(ThreadPoolJob, run) = ImageWorker_run;
} END_VIRTUAL


static int AFF4Image_finish(AFFObject this) {
  AFF4Image self = (AFF4Image)this;
  int result;

  AFF4_GL_LOCK;

  /* Update the size of the bevy */
  self->bevy_size = self->chunk_size * self->chunks_in_segment;

  switch(this->mode) {

  case 'w': {
    if(!self->stored) {
      RaiseError(EProgrammingError, "Image has no storage.");
      goto error;
    };

    /* Set sensible defaults */
    if(!self->chunk_size) {
      self->chunk_size = 32 * 1024;
    };

    if(!self->compression) {
      self->compression = ZIP_STORED;
    };

    if(!self->chunks_in_segment) {
      self->chunks_in_segment = 1024;
    };

    self->segment_count = 0;
    self->current = CONSTRUCT(ImageWorker, ImageWorker, Con, self, self, self->segment_count);

    if(self->thread_count <= 0) {
      self->thread_count = 1;
    };

    self->thread_pool = CONSTRUCT(ThreadPool, ThreadPool,
                                  Con, self, self->thread_count);

  }; break;

  default:
    RaiseError(EProgrammingError, "Unknown mode");
    goto error;
  };

  result = SUPER(AFFObject, FileLikeObject, finish);
  AFF4_GL_UNLOCK;
  return result;

 error:
  SUPER(AFFObject, FileLikeObject, finish);
  AFF4_GL_UNLOCK;
  return 0;
};


static int AFF4Image_write(FileLikeObject this, char *buffer, unsigned int length) {
  AFF4Image self = (AFF4Image) this;
  int offset = 0;

  AFF4_GL_LOCK;

  do {
    int need_to_write = length - offset;
    int availbale_to_write = self->bevy_size - self->current->bevy->size;

    if(need_to_write <= 0) break;

    CALL(self->current->bevy, write, buffer + offset,
         min(need_to_write, availbale_to_write));
    offset += availbale_to_write;

    if(self->current->bevy->size >= self->bevy_size) {
      /* Flush the worker to the thread pool and get a new one. */
      CALL(self->thread_pool, schedule, (ThreadPoolJob)self->current, 60);

      self->segment_count ++;
      self->current = CONSTRUCT(ImageWorker, ImageWorker, Con, self, self, self->segment_count);
    };
  } while(1);

  AFF4_GL_UNLOCK;
  return length;
};


static int _partial_read(FileLikeObject this, char *buffer, int length) {
  AFF4Image self = (AFF4Image)this;
  int segment_number = this->readptr / self->bevy_size;
  int segment_offset = this->readptr % self->bevy_size;

  /* Find the correct segment */
  RDFURN bevy = CALL(URNOF(self), copy, NULL);
  ZipFile zip = (ZipFile)CALL(RESOLVER, own, self->stored, 'r');
  FileLikeObject segment, index_segment;
  int availbale_to_read;


  CALL(bevy, add, talloc_asprintf(bevy, "%08X", segment_number));

  segment = CALL((AFF4Volume)zip, open_member, bevy, 'r', 0);

  CALL(bevy, add, "idx");
  index_segment = CALL((AFF4Volume)zip, open_member, bevy, 'r', 0);

  CALL(RESOLVER, cache_return, (AFFObject)zip);

  if(segment && index_segment) {
    int chunk_number = segment_offset / self->chunk_size;
    int chunk_offset = segment_offset % self->chunk_size;
    uint32_t chunk_data_offsets[self->chunks_in_segment];
    char uncompressed_buffer[self->chunk_size];
    int read_length;

    /* Read all the indexes into the chunk_data_offsets array */
    CALL(index_segment, read, (char *)&chunk_data_offsets,
         self->chunks_in_segment * sizeof(uint32_t));
    CALL(segment, seek, chunk_data_offsets[chunk_number], SEEK_SET);
    switch(self->compression) {

    case ZIP_DEFLATE: {
      /* Temporary storage for the compressed chunk. Zlib doesnt mind
         if we read a bit extra and this avoids us having to calculate
         the size of the chunks.
      */
      const Bytef compressed_chunk[self->chunk_size];
      int res;

      CALL(segment, read, (char *)compressed_chunk, self->chunk_size);

      AFF4_BEGIN_ALLOW_THREADS;

      // Try to decompress it:
      res = uncompress(uncompressed_buffer, &read_length, compressed_chunk,
                       (uLong)self->chunk_size);

      AFF4_END_ALLOW_THREADS;

      if(res != Z_OK ) {
        RaiseError(ERuntimeError, "Unable to decompress chunk %d", chunk_number);
        goto error;
      };
    }; break;

    case ZIP_STORED:
    default: {
      read_length = CALL(segment, read, uncompressed_buffer, self->chunk_size);
    }; break;
    };

    /* Now copy the partial buffer. */
    availbale_to_read = max(read_length - chunk_offset, length);
    memcpy(buffer, uncompressed_buffer + chunk_offset, availbale_to_read);
  };

  if(segment) {
    CALL((AFFObject)segment, close);
    talloc_free(segment);
  };

  if(index_segment) {
    CALL((AFFObject)index_segment, close);
    talloc_free(index_segment);
  };

  talloc_free(bevy);
  return availbale_to_read;

 error:
  talloc_free(bevy);
  return -1;
};


static int AFF4Image_read(FileLikeObject this, char *buffer, unsigned int length) {
  AFF4Image self = (AFF4Image)this;
  int offset = 0;

  AFF4_GL_LOCK;

  while(length > 0) {
    int res = _partial_read(this, buffer + offset, length);
    if(res == 0) break;

    offset += res;
    length -= res;
  };

  AFF4_GL_UNLOCK;
  return offset;
};


static int AFF4Image_close(AFFObject this) {
  AFF4Image self = (AFF4Image) this;

  AFF4_GL_LOCK;

  printf("About to flush last bevy.");

  /* Flush the last worker */
  CALL(self->thread_pool, schedule, (ThreadPoolJob)self->current, 60);

  /* Wait for all threads to finish */
  CALL(self->thread_pool, join);
  printf("Closing image.");
  fflush(stdout);

  AFF4_GL_UNLOCK;
  return 1;
};


VIRTUAL(AFF4Image, FileLikeObject) {
  VMETHOD_BASE(AFFObject, finish) = AFF4Image_finish;
  VMETHOD_BASE(AFFObject, close) = AFF4Image_close;
  VMETHOD_BASE(FileLikeObject, write) = AFF4Image_write;
  VMETHOD_BASE(FileLikeObject, read) = AFF4Image_read;
} END_VIRTUAL


AFF4_MODULE_INIT(A000_image) {
  INIT_CLASS(ImageWorker);

  register_type_dispatcher(AFF4_IMAGE, (AFFObject *)GETCLASS(AFF4Image));
};
