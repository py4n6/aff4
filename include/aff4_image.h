#include "aff4_internal.h"


/* This is the worker object itself (private) */
struct ImageWorker_t;

/** The Image Stream represents an Image in chunks */
CLASS(AFF4Image, FileLikeObject)
/* This is the volume where the image is stored */
  RDFURN stored;

  /* Chunks are cached here for faster randoom reading performance.
  */
  Cache chunk_cache;

  /* Thats the current worker we are using - when it gets full, we
     simply dump its bevy and take a new worker here.
  */
  struct ImageWorker_t *current;

  /* The thread pool that will be used to compress bevies. */
  ThreadPool thread_pool;

  /** Some parameters about this image */
  int chunk_size;
  int compression;
  int chunks_in_segment;
  uint32_t bevy_size;

  /* The current bevy we are working on. */
  int segment_count;

  EVP_MD_CTX digest;

  /* The number of threads to use in the threadpool. Set this before
     calling finish().
   */
  int thread_count;

END_CLASS
