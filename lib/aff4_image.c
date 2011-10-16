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

     // The index into the bevy - we use an IntegerArrayBinary
     // RDFValue, unless the array is very small in which case we
     // switch to the IntegerArrayInline
     StringIO index;

     ImageWorker METHOD(ImageWorker, Con, AFF4Image parent, int segment_count);

     // A write method for the worker
     int METHOD(ImageWorker, close);
END_CLASS


static ImageWorker ImageWorker_Con(ImageWorker self, AFF4Image parent, int segment_count) {
  self->image = parent;
  self->segment_count = segment_count;

  self->bevy = CONSTRUCT(StringIO, StringIO, Con, self);
  self->index = CONSTRUCT(StringIO, StringIO, Con, self);

  return self;
};


static void ImageWorker_run(ThreadPoolJob this) {
  ImageWorker self = (ImageWorker) this;

  printf("Will flush %d bytes in segment %d\n", self->bevy->size, self->segment_count);

  sleep(1);
};

static void ImageWorker_complete(ThreadPoolJob this) {
  ImageWorker self = (ImageWorker) this;
  RDFURN bevy_urn = CALL(URNOF(self->image), copy, self);
  ZipFile zip;
  FileLikeObject segment;
  Resolver resolver = ((AFFObject)(self->image))->resolver;

  CALL(bevy_urn, add, talloc_asprintf(bevy_urn, "%08X", self->segment_count));

  zip = (ZipFile)CALL(resolver, own, self->image->stored, 'w');
  segment = (FileLikeObject)CALL((AFF4Volume)zip, open_member, bevy_urn, 'w', ZIP_STORED);
  CALL(segment, write, self->bevy->data, self->bevy->size);

  CALL((AFFObject)segment, close);

  CALL(resolver, cache_return, (AFFObject)zip);
};



VIRTUAL(ImageWorker, ThreadPoolJob) {
  VMETHOD(Con) = ImageWorker_Con;
  VMETHOD_BASE(ThreadPoolJob, run) = ImageWorker_run;
  VMETHOD_BASE(ThreadPoolJob, complete) = ImageWorker_complete;
} END_VIRTUAL


static int AFF4Image_finish(AFFObject this) {
  AFF4Image self = (AFF4Image)this;

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
    return 0;
  };

  return SUPER(AFFObject, FileLikeObject, finish);

 error:
  SUPER(AFFObject, FileLikeObject, finish);
  return 0;
};


static int AFF4Image_write(FileLikeObject this, char *buffer, unsigned int length) {
  AFF4Image self = (AFF4Image) this;
  int offset = 0;

  do {
    int need_to_write = length - offset;
    int bevy_size = self->chunks_in_segment * self->chunk_size;
    int availbale_to_write = bevy_size - self->current->bevy->size;

    if(need_to_write <= 0) break;

    CALL(self->current->bevy, write, buffer + offset,
         min(need_to_write, availbale_to_write));
    offset += availbale_to_write;

    if(self->current->bevy->size >= bevy_size) {
      /* Flush the worker to the thread pool and get a new one. */
#if 1
      CALL(self->thread_pool, schedule, (ThreadPoolJob)self->current, 60);
#else
      CALL((ThreadPoolJob)self->current, run);
      CALL((ThreadPoolJob)self->current, complete);
      talloc_free(self->current);
#endif

      self->segment_count ++;
      self->current = CONSTRUCT(ImageWorker, ImageWorker, Con, self, self, self->segment_count);
    };
  } while(1);

  return length;
};


static int AFF4Image_close(AFFObject this) {
  AFF4Image self = (AFF4Image) this;

  printf("About to flush last bevy.");

  /* Flush the last worker */
#if 1
  CALL(self->thread_pool, schedule, (ThreadPoolJob)self->current, 60);
#else
  CALL((ThreadPoolJob)self->current, run);
  CALL((ThreadPoolJob)self->current, complete);
  talloc_free(self->current);
#endif

  /* Wait for all threads to finish */
  CALL(self->thread_pool, join);
  printf("Closing image.");
  fflush(stdout);
  return 1;
};


VIRTUAL(AFF4Image, FileLikeObject) {
  VMETHOD_BASE(AFFObject, finish) = AFF4Image_finish;
  VMETHOD_BASE(AFFObject, close) = AFF4Image_close;
  VMETHOD_BASE(FileLikeObject, write) = AFF4Image_write;
} END_VIRTUAL


AFF4_MODULE_INIT(A000_image) {
  INIT_CLASS(ImageWorker);

  register_type_dispatcher(AFF4_IMAGE, (AFFObject *)GETCLASS(AFF4Image));
};
