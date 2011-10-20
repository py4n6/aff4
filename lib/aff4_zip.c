/** This file implements the basic Zip handling code. We allow
    concurrent read/write.
*/
#include "aff4_internal.h"


/* Converts a URN to a relative and escaped name. */
static char *segment_name_from_URN(void *ctx, RDFURN urn, RDFURN container) {
  char *filename = CALL(urn, relative_name, container);
  filename = escape_filename(ctx, ZSTRING_NO_NULL(filename));

  return filename;
};


static AFFObject ZipSegment_Con(AFFObject this, RDFURN urn, char mode, Resolver resolver) {
  ZipSegment self = (ZipSegment)this;
  time_t now;
  struct tm *local_time;
  AFFObject result;

  AFF4_GL_LOCK;

  self->filename = new_XSDString(self);

  time(&now);
  local_time =  localtime(&now);

  // Fill in sensible defaults
  self->cd.magic = 0x2014b50;
  self->cd.version_made_by = 0x317;
  self->cd.version_needed = 0x14;

  // We always write trailing directory structures
  self->cd.flags = 0x8;
  self->cd.crc32 = 0;
  self->cd.file_size = 0;
  self->cd.compress_size = 0;

  self->cd.dosdate = (local_time->tm_year + 1900 - 1980) << 9 |
      (local_time->tm_mon + 1) << 5 | local_time->tm_mday;
  self->cd.dostime = local_time->tm_hour << 11 | local_time->tm_min << 5 |
      local_time->tm_sec / 2;

  self->cd.external_file_attr = 0644 << 16L;

  INIT_LIST_HEAD(&self->members);

  result = SUPER(AFFObject, FileLikeObject, Con, urn, mode, resolver);

  AFF4_GL_UNLOCK;

  return result;
};


static int ZipSegment_finish(AFFObject this) {
  ZipSegment self = (ZipSegment)this;
  int result;

  AFF4_GL_LOCK;

  // We use this as an anchor for data that can be disposed after close.
  self->buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  if(!self->container) {
    RaiseError(EProgrammingError, "Container not set for segment.");
    goto error;
  };

  self->cd.compression_method = self->compression_method;

  if(this->mode == 'w') {
    /* Compression_method specific initialization. */
    switch(self->compression_method) {
      case ZIP_DEFLATE:
        // Initialise the stream compressor
        memset(&self->strm, 0, sizeof(self->strm));
        self->strm.next_in = talloc_size(self->buffer, BUFF_SIZE);
        self->strm.next_out = talloc_size(self->buffer, BUFF_SIZE);

        if(deflateInit2(&self->strm, 9, Z_DEFLATED, -15,
                        9, Z_DEFAULT_STRATEGY) != Z_OK) {
          RaiseError(ERuntimeError, "Unable to initialise zlib (%s)", self->strm.msg);
          goto error;
        };
        break;

      case ZIP_STORED:
        break;

      default:
        RaiseError(ERuntimeError, "Compression method %d not supported",
                   self->compression_method);
        goto error;
    };
  };

  result = SUPER(AFFObject, AFF4Volume, finish);
  AFF4_GL_UNLOCK;
  return result;

 error:
  SUPER(AFFObject, AFF4Volume, finish);
  AFF4_GL_UNLOCK;
  return 0;
};


/**
   This zlib trickery comes from http://www.zlib.net/zlib_how.html
**/
static int ZipSegment_write(FileLikeObject self, char *buffer, unsigned int length) {
  ZipSegment this = (ZipSegment)self;
  int result = 0;

  AFF4_GL_LOCK;

  if(length == 0) goto exit;

  if(!this->buffer) {
    RaiseError(EIOError, "Can not write after clone.");
    goto error;
  }


  // Update the crc:
  this->cd.crc32 = crc32(this->cd.crc32,
                         (unsigned char*)buffer,
                         (unsigned int)length);

  // Is this compressed?
  switch(this->cd.compression_method) {
    case ZIP_DEFLATE: {
      unsigned char compressed[BUFF_SIZE];

      /** Give the buffer to zlib */
      this->strm.next_in = (unsigned char *)buffer;
      this->strm.avail_in = length;

      /** We spin here until zlib consumed all the data */
      do {
        int ret;

        this->strm.avail_out = BUFF_SIZE;
        this->strm.next_out = compressed;

        ret = deflate(&this->strm, Z_NO_FLUSH);
        ret = CALL(this->buffer, write, (char *)compressed,
                   BUFF_SIZE - this->strm.avail_out);
        if(ret<0) {
          RaiseError(EIOError, "Unable to write compressed data.");
          goto error;
        };
        result += ret;
      } while(this->strm.avail_out == 0);

      break;
    };

    case ZIP_STORED:
    default:
      /** Without compression, we just write the buffer right away */
      result = CALL(this->buffer, write, buffer, length);
  };

  /** Update our compressed size here */
  this->cd.compress_size += result;
  this->cd.file_size += length;

 exit:
  // Non error path
  ClearError();

  AFF4_GL_UNLOCK;
  return result;

 error:
  AFF4_GL_UNLOCK;
  return -1;
};


/* Read the entire segment into memory. */
static int decompress_segment(ZipSegment self) {
  AFFObject oself = (AFFObject)self;
  ZipFile zip = (ZipFile)CALL(oself->resolver, own, self->container, 'r');
  struct ZipFileHeader file_header;
  char filename[BUFF_SIZE];
  int length;

  if(!zip) {
    RaiseError(EIOError, "Unable to open container.");
    return 0;
  };

  CALL(zip->backing_store, seek, self->offset_of_file_header, SEEK_SET);
  CALL(zip->backing_store, read, (char *)&file_header, sizeof(file_header));

  /* Check the file header makes sense: */
  if(file_header.file_name_length != self->cd.file_name_length) {
    RaiseError(EIOError, "File header does not match CD record.");
    goto error;
  };

  length = CALL(zip->backing_store, read, filename,
                min(BUFF_SIZE, file_header.file_name_length));
  if(length != file_header.file_name_length)
    goto error;

  /* Check the filenames match. */
  if(memcmp(self->filename->value, filename, self->filename->length)) {
    RaiseError(EIOError, "Filename does not match CD record.");
    goto error;
  };

  // Make a new buffer.
  self->buffer = CONSTRUCT(StringIO, StringIO, Con, self);

  // Pre-allocate the buffer
  CALL(self->buffer, seek, self->cd.file_size, SEEK_SET);

  /* Depending on the compression_method we do different things here. */
  switch(file_header.compression_method) {

    /* Just read the data directly into the buffer. */
    case ZIP_STORED: {
      self->buffer->readptr = CALL(zip->backing_store, read, self->buffer->data,
                                   self->buffer->size);
    }; break;

    case ZIP_DEFLATE: {
      z_stream strm;
      unsigned char *cbuff = talloc_size(NULL, self->cd.compress_size);
      int length = CALL(zip->backing_store, read, (char *)cbuff, self->cd.compress_size);

      /** Set up our decompressor */
      strm.next_in = cbuff;
      strm.avail_in = length;
      strm.next_out = (unsigned char *)self->buffer->data;
      strm.avail_out = self->buffer->size;
      strm.zalloc = Z_NULL;
      strm.zfree = Z_NULL;

      if(inflateInit2(&strm, -15) != Z_OK) {
        RaiseError(ERuntimeError, "Failed to initialise zlib");
        talloc_free(cbuff);
        goto error;
      };

      if(inflate(&strm, Z_FINISH) != Z_STREAM_END ||     \
         strm.total_out != self->cd.file_size) {
        RaiseError(ERuntimeError, "Failed to fully decompress chunk (%s)", strm.msg);
        talloc_free(cbuff);
        goto error;
      };

      inflateEnd(&strm);
      talloc_free(cbuff);
    }; break;

    default:
      RaiseError(ERuntimeError, "Unknown compression method %d",
                 file_header.compression_method);
      goto error;
  };

  CALL(oself->resolver, cache_return, (AFFObject)zip);
  return 1;

error:
  CALL(oself->resolver, cache_return, (AFFObject)zip);
  return 0;
};


static int ZipSegment_read(FileLikeObject this, char *buffer, unsigned int length) {
  ZipSegment self = (ZipSegment)this;
  int result;

  AFF4_GL_LOCK;

  /* Decompress entire segment on demand. */
  if(!self->buffer && !decompress_segment(self)) {
    goto error;
  };

  CALL(self->buffer, seek, this->readptr, SEEK_SET);
  result = CALL(self->buffer, read, buffer, length);

  AFF4_GL_UNLOCK;
  return result;

error:
  AFF4_GL_UNLOCK;
  return -1;
};


/** This writes a zip64 end of central directory and a central
    directory locator */
static void write_zip64_CD(ZipFile self, StringIO fd, uint64_t offset_of_end_cd,
                           uint64_t directory_offset, int total_entries) {
  struct Zip64CDLocator locator;
  struct Zip64EndCD end_cd;

  locator.magic = 0x07064b50;
  locator.disk_with_cd = 0;
  locator.offset_of_end_cd = offset_of_end_cd;
  locator.number_of_disks = 1;

  memset(&end_cd, 0, sizeof(end_cd));
  end_cd.magic = 0x06064b50;
  end_cd.size_of_header = sizeof(end_cd)-12;
  end_cd.version_made_by = 0x2d;
  end_cd.version_needed = 0x2d;
  end_cd.number_of_entries_in_volume = total_entries;
  end_cd.number_of_entries_in_total = total_entries;
  end_cd.size_of_cd = locator.offset_of_end_cd - directory_offset;
  end_cd.offset_of_cd = directory_offset;

  DEBUG_OBJECT("writing ECD at %lld\n", fd->readptr);
  CALL(fd,write, (char *)&end_cd, sizeof(end_cd));
  CALL(fd,write, (char *)&locator, sizeof(locator));
};


/* Flush the new segment to our container. */
static int ZipSegment_close(AFFObject this) {
  ZipSegment self = (ZipSegment)this;
  uint32_t magic = 0x08074b50;
  struct ZipFileHeader header;
  char *filename;
  int result;
  // By owning the zip file we guarantee we are the only thread which is writing
  // to it now.
  ZipFile zip;

  AFF4_GL_LOCK;

  // Nothing to do for reading.
  if(this->mode == 'r')
    goto exit;

  zip = (ZipFile)CALL(this->resolver, own, self->container, 'w');

  if(!zip) {
    RaiseError(ERuntimeError, "Unable to get container.");
    goto error;
  };

  /* We append this file to the end of the zip file. */
  self->offset_of_file_header = CALL(zip->backing_store, seek, 0, SEEK_END);

  /* Finalize the compressor. */
  switch(self->cd.compression_method) {
    case ZIP_DEFLATE: {
      unsigned char compressed[BUFF_SIZE];

      do {
        int ret;

        self->strm.avail_out = BUFF_SIZE;
        self->strm.next_out = compressed;

        ret = deflate(&self->strm, Z_FINISH);
        ret = CALL(self->buffer, write, (char *)compressed,
                   BUFF_SIZE - self->strm.avail_out);

        self->cd.compress_size += ret;
      } while(self->strm.avail_out == 0);

      (void)deflateEnd(&self->strm);
    }; break;

    default:
      break;
  };

  // Make a filename suitable for a zip file.
  filename = segment_name_from_URN(NULL, URNOF(self), self->container);
  CALL(self->filename, set, ZSTRING(filename));
  talloc_free(filename);

  // Write a file header on
  memset(&header, 0, sizeof(header));
  header.magic = 0x4034b50;
  header.version = 0x14;
  // We prefer to write trailing directory structures
  header.flags = 0x08;

  header.compression_method = self->cd.compression_method;
  header.file_name_length = self->filename->length;
  header.lastmoddate = self->cd.dosdate;
  header.lastmodtime = self->cd.dostime;

  CALL(zip->backing_store, write,(char *)&header, sizeof(header));
  CALL(zip->backing_store, write, self->filename->value, self->filename->length);

  // Now write the file content
  CALL(zip->backing_store, write, self->buffer->data, self->buffer->size);

  /* Write the Zip64 data descriptor */
  // Write a signature:
  CALL(zip->backing_store, write, (char *)&magic, sizeof(magic));
  CALL(zip->backing_store, write, (char *)&self->cd.crc32,
       sizeof(self->cd.crc32));

  // Zip64 data descriptor (Segments will never be larger than 4G).
  CALL(zip->backing_store, write, (char *)&self->cd.compress_size,
       sizeof(self->cd.compress_size));
  CALL(zip->backing_store, write, (char *)&self->cd.file_size,
       sizeof(self->cd.file_size));

  // Signal that we are done
  talloc_free(self->buffer);
  self->buffer = NULL;

  // Now we can release the zip file
  CALL(((AFFObject)self)->resolver, cache_return, (AFFObject)zip);

exit:
  result = SUPER(AFFObject, FileLikeObject, close);
  AFF4_GL_UNLOCK;
  return result;

error:
  SUPER(AFFObject, FileLikeObject, close);
  AFF4_GL_UNLOCK;
  return 0;
};


VIRTUAL(ZipSegment, FileLikeObject) {
  VMETHOD_BASE(AFFObject, Con) = ZipSegment_Con;
  VMETHOD_BASE(AFFObject, finish) = ZipSegment_finish;

  VMETHOD_BASE(FileLikeObject, read) = ZipSegment_read;
  VMETHOD_BASE(FileLikeObject, write) = ZipSegment_write;
  VMETHOD_BASE(AFFObject, close) = ZipSegment_close;
} END_VIRTUAL;


VIRTUAL(AFF4Volume, AFFObject) {

} END_VIRTUAL;


static int find_EndCentralDirectory(ZipFile self) {
  char buffer[BUFF_SIZE];
  int length, i;
  char *comment;

  /* Find the End of Central Directory Record - We read about 4k of
     data and scan for the header from the end, just in case there is
     an archive comment appended to the end.
  */
  uint64_t directory_offset = CALL(self->backing_store, seek, -(int64_t)BUFF_SIZE,
                                   SEEK_END);

  memset(buffer, 0, BUFF_SIZE);
  length = CALL(self->backing_store, read, buffer, BUFF_SIZE - sizeof(uint32_t));

  if(length<0)
    goto error;

  // Scan the buffer backwards for an End of Central Directory magic
  for(i=length; i>0; i--) {
    if(*(uint32_t *)(buffer+i) == 0x6054b50) {
      break;
    };
  };

  // Not found
  if(i==0 || length - i < sizeof(*self->end))
    goto error;

  // This is now the offset to the end of central directory record
  directory_offset += i;
  self->end = (struct EndCentralDirectory *)talloc_memdup(self, buffer + i,
                                                          sizeof(*self->end));

  comment = buffer + i + sizeof(*self->end);

  // Is there a comment field? We expect the comment field to be
  // exactly a URN. If it is we can update our notion of the URN to
  // be the same as that.
  if(self->end->comment_len > 4 && !memcmp(comment, "aff4", 4)) {
    // Is it a fully qualified name?
    if(!memcmp(comment, ZSTRING_NO_NULL(FQN))) {
      // Update our URN from the comment.
      CALL(URNOF(self), set, comment);
    };
  };

  /* Handle extended Zip64 CD */
  directory_offset = self->end->offset_of_cd;
  if(self->end->offset_of_cd == -1) {
    struct Zip64CDLocator locator;
    struct Zip64EndCD end_cd;

    // We reposition ourselved just before the EndCentralDirectory to
    // find the locator:
    CALL(self->backing_store, seek, directory_offset - sizeof(locator), SEEK_SET);
    CALL(self->backing_store, read, (char *)&locator, sizeof(locator));

    if(locator.magic != 0x07064b50) {
      RaiseError(EIOError, "No valid Zip64CDLocator magic.");
      goto error;
    };

    if(locator.disk_with_cd != 0 || locator.number_of_disks != 1) {
      RaiseError(ERuntimeError, "Zip Files with multiple parts are not supported");
      goto error;
    };

    // Now the Zip64EndCD
    CALL(self->backing_store, seek, locator.offset_of_end_cd, SEEK_SET);
    CALL(self->backing_store, read, (char *)&end_cd, sizeof(end_cd));

    if(end_cd.magic != 0x06064b50) {
      RaiseError(EIOError, "No valid Zip64EndCD magic.");
      goto error;
    };

    if(end_cd.number_of_disk != 0 || end_cd.number_of_disk_with_cd != 0 ||
       end_cd.number_of_entries_in_volume != end_cd.number_of_entries_in_total) {
      RaiseError(ERuntimeError, "Zip Files with multiple parts are not supported");
      goto error;
    };

    self->end->total_entries_in_cd = end_cd.number_of_entries_in_total;
    directory_offset = end_cd.offset_of_cd;
  };


  // Reposition ourself to the start of the CD.
  CALL(self->backing_store, seek, directory_offset, SEEK_SET);
  return directory_offset;

error:
  RaiseError(EIOError, "Unable to find the end of central directory.");
  return 0;
};


static ZipSegment load_entry_from_CD(ZipFile self) {
  ZipSegment result = CONSTRUCT(ZipSegment, AFFObject, Con, self, NULL, 'r', RESOLVER);
  char buffer[BUFF_SIZE];
  int length;

  result->container = URNOF(self);

  // The length of the struct up to the filename
  // Only read up to the filename member
  if(sizeof(result->cd) !=
     CALL(self->backing_store, read, (char *)AS_BUFFER(result->cd)))
    goto error;

  // Does the magic match?
  if(result->cd.magic != 0x2014b50)
    goto error;

  // Now read the filename
  length = min(result->cd.file_name_length, BUFF_SIZE - 1);
  if(CALL(self->backing_store, read, buffer, length) != length)
    goto error;

  buffer[length] = 0;
  result->filename = unescape_filename(result, buffer);

  printf("Found %s\n", result->filename->value);

  // Parse the time from the CD
  {
    struct tm x = {
      .tm_year = (result->cd.dosdate>>9) + 1980 - 1900,
      .tm_mon = ((result->cd.dosdate>>5) & 0xF) -1,
      .tm_mday = result->cd.dosdate & 0x1F,
      .tm_hour = result->cd.dostime >> 11,
      .tm_min = (result->cd.dostime>>5) & 0x3F,
      .tm_sec = (result->cd.dostime&0x1F) * 2,
      //      .tm_zone = "GMT"
    };

    result->timestamp = mktime(&x);
  };

  //  parse_extra_field(self, fd, cd_header.extra_field_len);
  self->compression_method = result->cd.compression_method;

  // The following checks for zip64 values
  result->offset_of_file_header = result->cd.relative_offset_local_header;

  if(result->cd.relative_offset_local_header == -1) {
    result->offset_of_file_header = result->cd.relative_offset_local_header;
  };

  return result;

error:
  talloc_free(result);
  return NULL;
};


static int ZipFile_load_from_backing_store(ZipFile self) {
  int directory_offset = find_EndCentralDirectory(self);
  int i;

  if(!directory_offset) {
    goto error;
  };

  CALL(self->backing_store, seek, directory_offset, SEEK_SET);

  for(i=0; i<self->end->total_entries_in_cd; i++) {
    ZipSegment segment = load_entry_from_CD(self);
    if(segment) {
      list_add_tail(&segment->members, &self->members);
    };
  };

  return 1;

error:
  return 0;
};


static AFFObject ZipFile_Con(AFFObject this, RDFURN urn, char mode, Resolver resolver) {
  ZipFile self = (ZipFile)this;
  AFFObject result;

  AFF4_GL_LOCK;

  self->storage_urn = new_RDFURN(self);
  INIT_LIST_HEAD(&self->members);

  result = SUPER(AFFObject, AFF4Volume, Con, urn, mode, resolver);

  AFF4_GL_UNLOCK;
  return result;
};


static int ZipFile_finish(AFFObject this) {
  ZipFile self = (ZipFile)this;
  int result = 0;

  AFF4_GL_LOCK;
  ClearError();

  self->backing_store = (FileLikeObject)CALL(
      this->resolver, create, self->storage_urn, AFF4_FILE, this->mode);

  if(!self->backing_store || !CALL((AFFObject)self->backing_store, finish)) {
    RaiseError(EIOError, "Unable to open the backing store.");
    goto error;
  };

  /* Parse the file as a zip file. */
  ZipFile_load_from_backing_store(self);

  /* Get the cache to manage our locking */
  result = SUPER(AFFObject, AFF4Volume, finish);
  CALL(this->resolver, manage, (AFFObject)self);

  AFF4_GL_UNLOCK;
  return result;

 error:
  AFF4_GL_UNLOCK;
  return 0;
};


static FileLikeObject ZipFile_open_member(AFF4Volume this, RDFURN member, char mode,
                                          int compression_method) {
  ZipFile self = (ZipFile)this;
  ZipSegment result = NULL;
  char *segment_filename;

  AFF4_GL_LOCK;

  segment_filename = segment_name_from_URN(NULL, member, URNOF(self));

  /* Do we know about this segment already? */
  list_for_each_entry(result, &self->members, members) {
    if(!strcmp(result->filename->value, segment_filename)) {
      // Found it!
      goto exit;
    }
  };

  result = NULL;

  /* No, we need to create a new one. */
  if(mode == 'w') {
    result = (ZipSegment)CONSTRUCT(ZipSegment, AFFObject, Con, self, member, mode, RESOLVER);
    result->container = URNOF(self);
    result->compression_method = compression_method;
    CALL(result->filename, set, ZSTRING(segment_filename));

    if(!CALL((AFFObject)result, finish)) {
      talloc_free(result);
      goto error;
    };

    list_add_tail(&result->members, &self->members);
  };

 exit:
  talloc_free(segment_filename);

  AFF4_GL_UNLOCK;
  return (FileLikeObject)result;

 error:
  AFF4_GL_UNLOCK;
  return NULL;
};


/* Write the central directory on the end and finalize the zip file. */
static int ZipFile_close(AFFObject this) {
  ZipFile self = (ZipFile)this;
  ZipSegment segment;
  StringIO zip64_header;
  // We dont want to write small buffers
  StringIO buffer;
  struct EndCentralDirectory end;
  uint64_t start_of_cd, offset_of_end_cd;
  int total_entries = 0;
  int result;

  AFF4_GL_LOCK;

  // Nothing to do for reading.
  if(this->mode == 'r')
    goto exit;

  buffer = CONSTRUCT(StringIO, StringIO, Con, NULL);
  zip64_header = CONSTRUCT(StringIO, StringIO, Con, buffer);

  // Preallocate memory
  CALL(buffer,seek, 10*BUFF_SIZE, 0);
  CALL(buffer,truncate, 0);
  CALL(zip64_header, write, "\x01\x00\x00\x00", 4);

  AFF4_LOG(AFF4_LOG_MESSAGE, AFF4_SERVICE_ZIP_VOLUME, URNOF(this),
           "Closing ZipFile volume");

  start_of_cd = CALL(self->backing_store, seek, 0, SEEK_END);

  /* Iterate over all our members */
  list_for_each_entry(segment, &self->members, members) {
    /* We only consider segments which are closed. */
    if(!segment->buffer) {
      total_entries ++;

      // Flush the cache if needed
      if(buffer->readptr > BUFF_SIZE * 10) {
        CALL(self->backing_store, write, buffer->data, buffer->readptr);
        CALL(buffer, truncate, 0);
      };

      // We prepare the zip64 optional header.
      CALL(zip64_header, truncate, 4);
      if(segment->offset_of_file_header > ZIP64_LIMIT) {
        segment->cd.relative_offset_local_header = -1;
        CALL(zip64_header, write, (char *)&segment->offset_of_file_header,
             sizeof(uint64_t));
      } else {
        segment->cd.relative_offset_local_header = segment->offset_of_file_header;
      };

      // We need to append an extended zip64 header
      if(zip64_header->size > 4) {
        uint16_t *x = (uint16_t *)(zip64_header->data +2);

        // update the size of the extra field
        *x=zip64_header->size-4;
        segment->cd.extra_field_len = zip64_header->size;
      };

      // OK - write the cd header
      segment->cd.file_name_length = segment->filename->length;

      CALL(buffer, write, (char *)&segment->cd, sizeof(segment->cd));
      CALL(buffer, write, segment->filename->value, segment->filename->length);

      // And any optional headers
      if(zip64_header->size > 4) {
        CALL(buffer, write, zip64_header->data, zip64_header->size);
      };
    };
  };

  // Now write an end of central directory record
  memset(&end, 0, sizeof(end));
  end.magic = 0x6054b50;
  offset_of_end_cd = CALL(self->backing_store, tell) + buffer->size;
  end.size_of_cd = offset_of_end_cd - start_of_cd;

  if(start_of_cd > ZIP64_LIMIT) {
    end.offset_of_cd = -1;
    write_zip64_CD(self, buffer, offset_of_end_cd, start_of_cd, total_entries);
  } else {
    end.offset_of_cd = start_of_cd;
  };

  end.total_entries_in_cd_on_disk = total_entries;
  end.total_entries_in_cd = total_entries;
  end.comment_len = strlen(URNOF(self)->value)+1;

  // Make sure to add our URN to the comment field in the end
  CALL(buffer, write, (char *)&end, sizeof(end));
  CALL(buffer, write, (char *)URNOF(self)->value, end.comment_len);

  /* Flush the buffers */
  CALL(self->backing_store, write, buffer->data, buffer->readptr);
  talloc_free(buffer);

  result = SUPER(AFFObject, AFF4Volume, close);

  AFF4_GL_UNLOCK;
  return result;

exit:
  SUPER(AFFObject, AFF4Volume, close);
  AFF4_GL_UNLOCK;
  return 0;
};


VIRTUAL(ZipFile, AFF4Volume) {
  VMETHOD_BASE(AFF4Volume, open_member) = ZipFile_open_member;
  VMETHOD_BASE(AFFObject, Con) = ZipFile_Con;
  VMETHOD_BASE(AFFObject, close) = ZipFile_close;
  VMETHOD_BASE(AFFObject, finish) = ZipFile_finish;
} END_VIRTUAL;


AFF4_MODULE_INIT(A000_zip) {
  register_type_dispatcher(AFF4_ZIP_VOLUME, (AFFObject *)GETCLASS(ZipFile));
};
