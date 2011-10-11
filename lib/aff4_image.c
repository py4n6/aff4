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
