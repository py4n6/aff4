/** This file implements the basic Zip handling code. We allow
    concurrent read/write.

    FIXME: The FileBackedObject needs to be tailored for windows.
*/
#include "aff4_internal.h"


static ZipSegment ZipSegment_Con(ZipSegment self, RDFURN urn, RDFURN container,
                                     char mode, Resolver resolver) {
  time_t now;
  struct tm *local_time;

  // We use this as an anchor for data that can be disposed after close.
  self->buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  self->container = (RDFURN)CALL((RDFValue)container, clone, self->buffer);

  time(&now);
  local_time =  localtime(&now);

  // Fill in sensible defaults
  self->cd.magic = 0x2014b50;
  self->cd.version_made_by = 0x317;
  self->cd.version_needed = 0x14;
  self->cd.compression_method = ZIP_STORED;

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

  // Initialise the stream compressor
  memset(&self->strm, 0, sizeof(self->strm));
  self->strm.next_in = talloc_size(self->buffer, BUFF_SIZE);
  self->strm.next_out = talloc_size(self->buffer, BUFF_SIZE);

  if(deflateInit2(&self->strm, 9, Z_DEFLATED, -15,
                  9, Z_DEFAULT_STRATEGY) != Z_OK) {
    RaiseError(ERuntimeError, "Unable to initialise zlib (%s)", self->strm.msg);
    goto error;
  };

  return (ZipSegment)SUPER(AFFObject, ZipSegment, Con, urn, mode, resolver);

error:
  talloc_free(self);
  return NULL;
};


/**
   This zlib trickery comes from http://www.zlib.net/zlib_how.html
**/
static int ZipSegment_write(FileLikeObject self, char *buffer, unsigned int length) {
  ZipSegment this = (ZipSegment)self;
  int result = 0;

  if(length == 0) goto exit;

  if(!this->buffer) {
    RaiseError(EIOError, "Can not write after clone.");
    return -1;
  }


  // Update the crc:
  this->cd.crc32 = crc32(this->cd.crc32,
                         (unsigned char*)buffer,
                         (unsigned int)length);

  // Is this compressed?
  if(this->cd.compression_method == ZIP_DEFLATE) {
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
      if(ret<0) return ret;
      result += ret;
    } while(this->strm.avail_out == 0);

  } else {
    /** Without compression, we just write the buffer right away */
    result = CALL(this->buffer, write, buffer, length);
  };

  /** Update our compressed size here */
  this->cd.compress_size += result;
  this->cd.file_size += length;

exit:
  // Non error path
  ClearError();
  return result;
};

/* Flush the new segment to our container. */
static int ZipSegment_close(AFFObject this) {
  ZipSegment self = (ZipSegment)this;
  uint32_t magic = 0x08074b50;
  struct ZipFileHeader header;
  char *filename;

  // By owning the zip file we guarantee we are the only thread which is writing
  // to it now.
  ZipFile zip = (ZipFile)CALL(this->resolver, own, self->container);

  if(!zip) {
    RaiseError(ERuntimeError, "Unable to get container.");
    goto error;
  };

  // Make a filename suitable for a zip file.
  filename = CALL(URNOF(self), relative_name, URNOF(zip));
  filename = escape_filename(self, ZSTRING_NO_NULL(filename));

  /* We append this file to the end of the zip file. */
  self->offset_of_file_header = CALL(zip->backing_store, seek, 0, SEEK_END);

  /* Finalize the compressor. */
  if(self->cd.compression_method == ZIP_DEFLATE) {
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
  };

  // Write a file header on
  memset(&header, 0, sizeof(header));
  header.magic = 0x4034b50;
  header.version = 0x14;
  // We prefer to write trailing directory structures
  header.flags = 0x08;

  header.compression_method = self->cd.compression_method;
  header.file_name_length = strlen(filename);
  header.lastmoddate = self->cd.dosdate;
  header.lastmodtime = self->cd.dostime;

  CALL(zip->backing_store, write,(char *)&header, sizeof(header));
  CALL(zip->backing_store, write, ZSTRING_NO_NULL(filename));

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

  return 1;

error:
  return 0;
};


VIRTUAL(ZipSegment, FileLikeObject) {
  VMETHOD(Con) = ZipSegment_Con;

  VMETHOD_BASE(FileLikeObject, write) = ZipSegment_write;
  VMETHOD_BASE(AFFObject, close) = ZipSegment_close;
} END_VIRTUAL;


VIRTUAL(AFF4Volume, AFFObject) {

} END_VIRTUAL;


static AFFObject ZipFile_Con(AFFObject this, RDFURN urn, char mode, Resolver resolver) {
  ZipFile self = (ZipFile)this;

  self->storage_urn = new_RDFURN(self);
  self->members = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, 0);

  return SUPER(AFFObject, AFFObject, Con, urn, mode, resolver);
};

static int ZipFile_finish(AFFObject this) {
  ZipFile self = (ZipFile)this;
  ClearError();

  self->backing_store = (FileLikeObject)CALL(
      this->resolver, create, self->storage_urn, AFF4_FILE);

  if(!self->backing_store || !CALL((AFFObject)self->backing_store, finish)) {
    RaiseError(EIOError, "Unable to open the backing store.");
    return 0;
  };

  return SUPER(AFFObject, AFF4Volume, finish);
};


static FileLikeObject ZipFile_open_member(AFF4Volume this, RDFURN member, char mode,
                                          int compression) {
  ZipFile self = (ZipFile)this;

  ZipSegment result = CONSTRUCT(ZipSegment, ZipSegment, Con, self, member, URNOF(self), mode,
                                RESOLVER);

  CALL(self->members, put, ZSTRING(member->value), (Object)result);

  return (FileLikeObject)result;
};

VIRTUAL(ZipFile, AFF4Volume) {
  VMETHOD_BASE(AFF4Volume, open_member) = ZipFile_open_member;
  VMETHOD_BASE(AFFObject, Con) = ZipFile_Con;
  VMETHOD_BASE(AFFObject, finish) = ZipFile_finish;
} END_VIRTUAL;


AFF4_MODULE_INIT(A000_zip) {
  register_type_dispatcher(AFF4_ZIP_VOLUME, (AFFObject *)GETCLASS(ZipFile));
};
