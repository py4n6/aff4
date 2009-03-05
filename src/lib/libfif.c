#include "fif.h"
#include "time.h"
#include <uuid/uuid.h>
#include <libgen.h>

/** Dispatch and construct the stream from its type. Stream will be    
    opened for writing. Callers should repeatadly write on it and must
    call close() before a new stream can be opened.
**/
static AFFFD FIFFile_create_stream_for_writing(FIFFile self, char *stream_name,
					char *stream_type, Properties props) {
#if 0

  int i;
  // The new stream will get a new UUID
  char *stream_uuid = new_uuid(self);

  // We let the resolver know that we are found within this current
  // volume:
  char buffer[BUFF_SIZE];
  char *value = talloc_asprintf(self, "urn:aff2:%s", self->uuid);

  // This says that stream_uuid is found in the volume self->uuid
  snprintf(buffer, BUFF_SIZE, "urn:aff2:%s:aff2:volume", stream_uuid);  
  CALL(self->resolver, add, buffer, ZSTRING_NO_NULL(value));
  
  // Add same to the global property file (this volume contains
  // stream_uuid):
  CALL(self->props, add, "aff2:contains", 
       talloc_asprintf(self, "urn:aff2:%s", stream_uuid),0);
  
  // We can setup a link to the stream using its friendly name:
  if(stream_name) {
    char *stream_urn = talloc_asprintf(self, "urn:aff2:%s", stream_uuid);

    // This says that the volume self->uuid contains a link to
    // stream_name which is found in the stream stream_name
    snprintf(buffer, BUFF_SIZE, "urn:aff2:%s:%s:aff2:link",
	     self->uuid, stream_name);
    
    CALL(self->resolver, add, buffer, ZSTRING_NO_NULL(stream_urn));   
    // Add same to the global property file:
    CALL(self->props, add, buffer, talloc_strdup(self,value),0);
  };
  
  for(i=0; dispatch[i].type !=NULL; i++) {
    if(!strcmp(dispatch[i].type, stream_type)) {
      AFFFD result;

      // A special constructor from a class reference
      result = CONSTRUCT_FROM_REFERENCE(dispatch[i].class_ptr, 
					Con, self, stream_uuid, props, self);
      return result;
    };
  };
#endif

  RaiseError(ERuntimeError, "No handler for stream_type '%s'", stream_type);
  return NULL;
};

static void FIFFile_create_new_volume(ZipFile self, FileLikeObject file) {
  FIFFile this=(FIFFile)self;

  // Get a new URN for this volume:
  self->super.Con(self, NULL);

  // We are about to open a new volume - record that
  CALL(this->resolver, add, 
       talloc_strdup(NULL, self->super.uri),	       /* Source URI */
       talloc_strdup(NULL, "aff2:stored"),   /* Attributte */
       talloc_strdup(NULL, file->super.uri));  /* Value */

  CALL(this->resolver, add, 
       talloc_strdup(NULL, self->super.uri),	       /* Source URI */
       talloc_strdup(NULL, "aff2:type"),   /* Attributte */
       talloc_strdup(NULL, "volume"));  /* Value */
  
  this->__super__->create_new_volume(self, file);
};

static AFFFD FIFFile_open_stream(FIFFile self, char *stream_name) {
  int length=0;
  char *buffer;
  Properties props;
  char *stream_type;
  int i;
  char *path = talloc_asprintf(self, "%s/properties", stream_name);

#if 0
  buffer = self->super.read_member((ZipFile)self, path, &length);
  if(!buffer) {
    RaiseError(ERuntimeError, "Stream %s does not have properties?", stream_name);
    return NULL;
  };

  props = CONSTRUCT(Properties, Properties, Con, self);  
  CALL(props, parse, buffer,length, 0);

  // We want to get the type of this stream
  stream_type = CALL(props, iter_next, NULL, "type");
  if(!stream_type) {
    RaiseError(ERuntimeError, "Stream does not have a type property?");
    talloc_free(props);
    return NULL;
  };

  // Find it in the dispatcher struct and instantiate it
  for(i=0; dispatch[i].type !=NULL; i++) {
    if(!strcmp(dispatch[i].type, stream_type)) {
      AFFFD result;
      char *tmp = CALL(props, iter_next, NULL, "size");

      // A special constructor from a class reference
      result = CONSTRUCT_FROM_REFERENCE(dispatch[i].class_ptr, 
					Con, self, stream_name, props, self);

      // Ensure the stream is of the right size as set by the
      // properties:
      result->super.size = strtoll(tmp, NULL, 0);

      return result;
    };
  };

#endif
  
  RaiseError(ERuntimeError, "This implementation is unable to open streams of type %s", 
	     stream_type);
  
  return NULL;
};

static int FIFFile_destructor(void *self) {
  ZipFile this = (ZipFile)self;
  CALL(this, close);

  return 0;
};

static ZipFile FIFFile_Con(ZipFile self, FileLikeObject fd) {
  FIFFile this = (FIFFile)self;
  Cache i;

  // This is our resolver
  this->resolver = CONSTRUCT(Resolver, Resolver, Con, self);

  // Open the file now
  self = this->__super__->Con(self, fd);

  // Now tell the resolver about everything we know:
  list_for_each_entry(i, &self->zipinfo_cache->cache_list, cache_list) {
    ZipInfo zip = (ZipInfo)i->data;

    CALL(this->resolver, add,
	 talloc_strdup(NULL, zip->filename),
	 talloc_strdup(NULL, "aff2:location"),
	 talloc_strdup(NULL, self->super.uri));

    CALL(this->resolver, add,
	 talloc_strdup(NULL, zip->filename),
	 talloc_strdup(NULL, "aff2:type"),
	 talloc_strdup(NULL, "blob"));

  };

  return self;
};

/** This method gets called whenever the CD parsing finds a new
    archive member. We parse any properties files we find.
*/
static void FIFFile_add_zipinfo_to_cache(ZipFile self, ZipInfo zip) {
  FIFFile this = (FIFFile)self;
  int filename_length = strlen(zip->filename);
  int properties_length = strlen("properties");
  
  // Call our baseclass
  this->__super__->add_zipinfo_to_cache(self,zip);

  // We identify streams by their filename ending with "properties"
  // and parse out their properties:
  if(filename_length >= properties_length && 
     !strcmp("properties", zip->filename + filename_length - properties_length)) {
    int len;
    char *text;
    int i,j,k;
    // Make our own local copy so we can modify it (the original is
    // cached and doesnt belong to us).
    char *tmp;
    char *tmp_text;
    char *source;
    char *attribute;
    char *value;

    text = CALL(self, read_member, zip->filename, &len);
    if(!text) return;

    tmp = talloc_memdup(self, text, len+1);
    tmp[len]=0;
    tmp_text = tmp;

    // Find the next line:
    while((i=strcspn(tmp_text, "\r\n"))) {
      tmp_text[i]=0;
      
      // Locate the =
      j=strcspn(tmp_text,"=");
      if(j==i) goto exit;
      
      tmp_text[j]=0;
      value = tmp_text + j + 1;

      // Locate the space
      k=strcspn(tmp_text," ");
      if(k==j) {
	// No absolute URN specified, we use the current filename:
	source = talloc_strdup(tmp, zip->filename);
	source = dirname(source);
	attribute = tmp_text;
      } else {
	source = tmp_text;
	attribute = tmp_text + k+1;
      };
      tmp_text[k]=0;

      if(strlen(source)<10 || memcmp(source, ZSTRING_NO_NULL("urn:aff2:"))) {
	source = talloc_asprintf(tmp, "urn:aff2:%s", source);
      };
      
      if(strlen(attribute)<4 || memcmp(attribute, ZSTRING_NO_NULL("aff2:"))) {
	attribute = talloc_asprintf(tmp, "aff2:%s",attribute);
      }

      // Try to recognise a statement like URN stored . (URN stored
      // here) to find the current volumes UUID.
      if((!strcmp(value,".") || !strcmp(value,self->fd->super.uri)) && 
	  !strcmp(attribute,"aff2:stored")) {
	value = self->fd->super.uri;
	self->super.uri = talloc_strdup(self, source);
      };

      // Now add to the global resolver (These will all be possibly
      // stolen).
      CALL(this->resolver, add, 
	   talloc_strdup(tmp, source),
	   talloc_strdup(tmp, attribute),
	   talloc_strdup(tmp, value));

      // Move to the next line
      tmp_text = tmp_text + i+1;
    };
    
  printf("Found property file %s\n%s", zip->filename, text);

 exit:
  talloc_free(tmp);
  };

};

static void FIFFile_close(ZipFile self) {
  // Force our properties to be written
  FIFFile this = (FIFFile)self;
  char *str;
  if(self->_didModify) {
    // Export the volume properties here
    str = CALL(this->resolver, export, self->super.uri);
    if(str) {
      CALL(self, writestr, "properties", ZSTRING_NO_NULL(str),
	   NULL,0, 0);
      talloc_free(str);
    };
  };

  // Call our base class:
  this->__super__->close(self);
};

/** This is the constructor which will be used when we get
    instantiated as an AFFObject.
*/
AFFObject FIFFile_AFFObject_Con(AFFObject self, char *urn) {
  FIFFile this = (FIFFile)self;

  if(urn) {
    // Ok, we need to create ourselves from a URN. We need a
    // FileLikeObject first. We ask the oracle what object should be
    // used as our underlying FileLikeObject:
    Resolver oracle = CONSTRUCT(Resolver, Resolver, Con, self);
    char *url = CALL(oracle, resolve, urn, "aff2:stored");
    FileLikeObject fd;

    // We have no idea where we are stored:
    if(!url) {
      RaiseError(ERuntimeError, "Can not find the storage for Volume %s", urn);
      goto error;
    };

    // Ask the oracle for the file object
    fd = (FileLikeObject)CALL(oracle, open, url, "FileLikeObject");
    if(!fd) goto error;

    // Call our other constructor to actually read this file:
    self = this->super.Con((ZipFile)this, fd);
  } else {
    // Call ZipFile's AFFObject constructor.
    this->__super__->super.Con(self, urn);
  };

  return self;
 error:
  talloc_free(self);
  return NULL;
};

VIRTUAL(FIFFile, ZipFile)
     VMETHOD(create_stream_for_writing) = FIFFile_create_stream_for_writing;
     VMETHOD(open_stream)  = FIFFile_open_stream;
     VMETHOD(super.close) = FIFFile_close;
     VMETHOD(super.add_zipinfo_to_cache) = FIFFile_add_zipinfo_to_cache;
     VMETHOD(super.create_new_volume) = FIFFile_create_new_volume;

// There are two constructors:
     VMETHOD(super.super.Con) = FIFFile_AFFObject_Con;
     VMETHOD(super.Con) = FIFFile_Con;
END_VIRTUAL

static AFFFD AFFFD_Con(AFFFD self, char *stream_name, Properties props, FIFFile parent) {
  char *tmp;

  INIT_LIST_HEAD(&self->list);

  printf("Constructing an image %s\n", stream_name);
  if(!props) {
    props = CONSTRUCT(Properties, Properties, Con, self);
  } else{
    talloc_steal(self, props);
  };

  self->props = props;

  // Ensure that we have the type in there
  CALL(props, del, "type");
  CALL(props, add, "type", self->type, time(NULL));

  tmp = CALL(props, iter_next, NULL, "size");
  if(tmp) {
    self->super.size = strtoll(tmp, NULL, 0);
  } else {
    self->super.size = 0;
  };

  self->parent = parent;

  // Take the stream name provided to us:
  self->stream_name = stream_name;
  talloc_steal(self, stream_name);

  return self;
};

static void AFFFD_close(FileLikeObject self) {
  AFFFD this = (AFFFD)self;
  // Write out a properties memeber:
  char *str = CALL(this->props, str);
  char *filename = talloc_asprintf(str, "%s/properties", this->stream_name);

  this->parent->super.writestr((ZipFile)this->parent, 
			       filename, ZSTRING_NO_NULL(str),
			       NULL, 0, 0);

  // Call our baseclass
  this->__super__->close(self);
};

static char *AFFFD_get_uri(FileLikeObject self) {
  AFFFD this = (AFFFD)self;
  
  return talloc_asprintf(self, "uri:aff2:%s", this->stream_name);
};

VIRTUAL(AFFFD, FileLikeObject)
END_VIRTUAL

#if 0

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

static AFFFD Image_Con(AFFFD self, char *uri) {
  Image this = (Image)self;
  char *tmp;

  // Call our baseclass
  this->__super__->Con(self, uri);

  // Set the defaults
  tmp = CALL(props, iter_next, NULL, "chunks_in_segment");
  if(!tmp) tmp = CALL(props, add, "chunks_in_segment", "2048", time(NULL));
  this->chunks_in_segment = strtol( tmp, NULL, 0);

  tmp = CALL(props, iter_next, NULL, "chunk_size");
  if(!tmp) tmp = CALL(props, add, "chunk_size", "0x8000", time(NULL));

  this->chunk_size = strtol(tmp, NULL,0);

  this->chunk_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  this->segment_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  this->chunk_count = 0;
  this->segment_count =0;

  this->chunk_indexes = talloc_array(self, int32_t, this->chunks_in_segment);
  // Fill it with -1 to indicate an invalid pointer
  memset(this->chunk_indexes, 0xff, sizeof(int32_t) * this->chunks_in_segment);

  // Initialise the chunk cache:
  this->chunk_cache = CONSTRUCT(Cache, Cache, Con, self, HASH_TABLE_SIZE, CACHE_SIZE);

  // We need to use slightly different hashing and cmp operations as
  // we will be using the chunk_id (int) as a key:
  this->chunk_cache->hash = cache_hash_int;
  this->chunk_cache->cmp = cache_cmp_int;

  return self;
};

/** This is how we implement the Image stream writer:

As new data is written we append it to the chunk buffer, then we
remove chunk sized buffers from it and push them to the segment. When
the segment is full we dump it to the parent container set.
**/
static int dump_chunk(Image this, char *data, uint32_t length, int force) {
  // We just use compress() to get the compressed buffer.
  char cbuffer[compressBound(length)];
  int clength=compressBound(length);

  memset(cbuffer,0,clength);

  // Should we even offer to store chunks uncompressed?
  if(0) {
    memcpy(cbuffer, data, length);
    clength = length;
  } else {
    if(compress((unsigned char *)cbuffer, (unsigned long int *)&clength, 
		(unsigned char *)data, (unsigned long int)length) != Z_OK) {
      RaiseError(ERuntimeError, "Compression error");
      return -1;
    };
  };
  
  // Update the index to point at the current segment stream buffer
  // offset
  this->chunk_indexes[this->chunk_count] = this->segment_buffer->readptr;
  
  // Now write the buffer to the stream buffer
  CALL(this->segment_buffer, write, cbuffer, clength);
  this->chunk_count ++;
  
  // Is the segment buffer full? If it has enough chunks in it we
  // can flush it out:
  if(this->chunk_count >= this->chunks_in_segment || force) {
    char tmp[BUFF_SIZE];
    // Format the segment name
    snprintf(tmp, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, this->super.stream_name, 
	     this->segment_count);

    // Store the entire segment in the zip file
    this->super.parent->super.writestr((ZipFile)this->super.parent, 
				       tmp, this->segment_buffer->data, 
				       this->segment_buffer->readptr, 
				       // Extra field
				       (char *)this->chunk_indexes,
				       (this->chunk_count) * sizeof(uint32_t),
				       // No compression for segments
				       ZIP_STORED
				       );
    
    // Reset everything to the start
    CALL(this->segment_buffer, truncate, 0);
    memset(this->chunk_indexes, -1, sizeof(int32_t) * this->chunks_in_segment);
    this->chunk_count =0;
    // Next segment
    this->segment_count ++;
  };
  
  return clength;
};

static int Image_write(FileLikeObject self, char *buffer, unsigned long int length) {
  Image this = (Image)self;

  CALL(this->chunk_buffer, seek, 0, SEEK_END);
  CALL(this->chunk_buffer, write, buffer, length);
  while(this->chunk_buffer->readptr > this->chunk_size) {
    if(dump_chunk(this, this->chunk_buffer->data, this->chunk_size, 0)<0)
      return -1;
      
    // Consume the chunk from the chunk_buffer
    CALL(this->chunk_buffer, skip, this->chunk_size);
  };

  self->readptr += length;
  self->size = max(self->readptr, self->size);

  return length;
};

static void Image_close(FileLikeObject self) {
  Image this = (Image)self;
  AFFFD pthis = (AFFFD)self;
  char tmp[BUFF_SIZE];

  // Write the last chunk
  dump_chunk(this, this->chunk_buffer->data, this->chunk_buffer->readptr, 1);

  // Set the stream size
  CALL(pthis->props, del, "size");
  snprintf(tmp, BUFF_SIZE, "%lld", self->size);
  CALL(pthis->props, add, "size", tmp, time(NULL)); 

  this->__super__->super.close(self);
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
  char compressed_chunk[this->chunk_size + 100];
  int compressed_length;

  char *uncompressed_chunk;
  int uncompressed_length=this->chunk_size + 1024;

  /** Now we need to figure out where the segment is */
  char *filename;
  ZipInfo zip;

  /** This holds the index into the segment of all the chunks.
      It is an array of this->chunks_in_segment ints long. It may be
      cached.
  */
  int32_t *chunk_index;
  struct ZipFileHeader header;
  Cache extra,chunk_cache;

  // Is the chunk cached? If it is - we can just copy a subset of it
  // on the result and get out of here....
  chunk_cache = CALL(this->chunk_cache, get, &chunk_id);
  if(chunk_cache) {
    available_to_read = min(available_to_read, chunk_cache->data_len);

    // Copy it on the result stream
    length = CALL(result, write, chunk_cache->data + chunk_offset, 
		  available_to_read);
    
    self->readptr += length;

    return length;
  };

  // Prepare a buffer for decompressing - it will be committed to the
  // cache later on:
  uncompressed_chunk = talloc_size(NULL, uncompressed_length);

  // What is the segment name?
  filename = talloc_asprintf(self, IMAGE_SEGMENT_NAME_FORMAT, this->super.stream_name,
			     segment_id);

  // We are only after the extra field of this segment - if thats
  // already cached we dont need to re-read it:
  extra = CALL(this->super.parent->super.extra_cache, get, filename);

  // Get the ZipInfo struct for this filename
  zip = this->super.parent->super.fetch_ZipInfo((ZipFile)this->super.parent,
						filename);
  
  // Just interpolate NULLs if the segment is missing
  if(!zip) {
    RaiseError(ERuntimeError, "Segment %s for offset %lld is missing, padding", 
	       filename, self->readptr);
    goto error;
  };
  
  // Is the extra cached? If so that is the chunk index
  if(extra) {    
    chunk_index = (int32_t *)extra->data;

    // No - we have to do it the slow way
  } else {
    // Fetch the extra field from the zip file header - this is the
    // chunk index into the segment: 
    CALL(zip->fd, seek, zip->cd_header.relative_offset_local_header,
	 SEEK_SET);
    if(CALL(zip->fd, read,(char *)&header, sizeof(header)) != sizeof(header)) {
      RaiseError(ERuntimeError, "Cant read FileHeader for %s", filename);
      goto error;
    };
  
    // Is there an extra field - there has to be for a valid segment
    if(header.extra_field_len==0) {
      RaiseError(ERuntimeError, "Cant find the extra field for segment %s", filename);
      goto error;
    };
    
    // Read the filename (A bit extra allocation for null
    // termination).
    filename = talloc_realloc(self, filename, char, header.file_name_length + 2);
    CALL(zip->fd, read, filename, header.file_name_length);
    
    // Check its what we expected - Zip files may have inconsistent
    // CD/FileHeader entries - we refuse to work with such files.
    if(memcmp(filename, zip->filename, zip->cd_header.file_name_length)) {
      RaiseError(ERuntimeError, "Filenames dont match in Central "
		 "Directory (%s) and File Header (%s)...", zip->filename, filename); \
      goto error;
    };

    // Make a new index - initialised to 0xFFFFFF (Thats our bad block
    // flag).
    chunk_index = talloc_array(zip, int32_t, this->chunks_in_segment);
    memset(chunk_index, 0xFF, this->chunks_in_segment * sizeof(uint32_t *));
    
    // Read the extra header:
    if(CALL(zip->fd, read, (char *)chunk_index, header.extra_field_len)	\
       != header.extra_field_len) {
      RaiseError(ERuntimeError, "Cant read extra field for segment %s", filename);
      goto error;
    };
    
    // Cache the extra for next time
    CALL(this->super.parent->super.extra_cache, put, filename, chunk_index, 
	 this->chunks_in_segment * sizeof(uint32_t *));

    // Note the location of the file
    zip->file_offset = zip->fd->readptr;
  };
  
  /** By here we have the chunk_index which is an array of offsets
      into the segment where the chunks begin. We work out the size of
      the chunk by subtracting the next chunk offset from the current
      one - except for the last one.
  */
  if(chunk_index_in_segment < this->chunks_in_segment &&	\
     chunk_index[chunk_index_in_segment+1] != -1) {
    compressed_length = chunk_index[chunk_index_in_segment+1] - \
      chunk_index[chunk_index_in_segment];
  } else {
    compressed_length = zip->cd_header.file_size - \
      chunk_index[chunk_index_in_segment];
  };

  // Fetch the compressed chunk
  CALL(zip->fd, seek, chunk_index[chunk_index_in_segment] + zip->file_offset,
       SEEK_SET);

  CALL(zip->fd, read, compressed_chunk, compressed_length);

  // Try to decompress it:
  if(uncompress((unsigned char *)uncompressed_chunk, 
		(unsigned long int *)&uncompressed_length, 
		(unsigned char *)compressed_chunk,
		(unsigned long int)compressed_length) != Z_OK ) {
    RaiseError(ERuntimeError, "Unable to decompress chunk %d", chunk_id);
    goto error;
  };

  // Copy it on the output stream
  length = CALL(result, write, uncompressed_chunk + chunk_offset, 
		available_to_read);

  self->readptr += length;

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

#endif

VIRTUAL(Image, AFFFD)
END_VIRTUAL

#if 0

VIRTUAL(Image, AFFFD)
     VMETHOD(super.Con) = Image_Con;
     VMETHOD(super.super.write) = Image_write;
     VMETHOD(super.super.read) = Image_read;
     VMETHOD(super.super.close) = Image_close;

     VATTR(super.type) = "Image";
END_VIRTUAL


/** We assume text is a null terminated string here */
static int Properties_parse(Properties self, char *text, uint32_t len, uint32_t date) {
  int i,j;
  // Make our own local copy so we can modify it
  char *tmp = talloc_memdup(self, text, len+1);
  char *tmp_text = tmp;

  // Absolute urns are added to the global resolver
  Resolver resolver=CONSTRUCT(Resolver, Resolver, Con, tmp);

  tmp[len]=0;

  // Find the next line:
  while((i=strcspn(tmp_text, "\r\n"))) {
    tmp_text[i]=0;

    j=strcspn(tmp_text,"=");
    if(j==i) goto exit;
    
    tmp_text[j]=0;
    CALL(self, add, tmp_text, tmp_text+j+1, date);
    if(j>10 && !memcmp(tmp_text, ZSTRING_NO_NULL("urn:aff2:"))) {
      // Its an absolute reference we will add to the global resolver:
      CALL(resolver, add, tmp_text, ZSTRING_NO_NULL(talloc_strdup(NULL, tmp_text+j+1)));
    };

    tmp_text += i+1;
  };

 exit:
  talloc_free(tmp);
  return 0;
};

static char *Properties_str(Properties self) {
  char *result = NULL;
  Properties i;

  list_for_each_entry(i, &self->list, list) {
    result = talloc_asprintf_append(result, "%s=%s\n", i->key, i->value);
  };

  talloc_steal(self, result);
  return result;
};

#endif

VIRTUAL(Properties, Object)
END_VIRTUAL

#if 0
/*** This is the implementation of the MapDriver */
AFFFD MapDriver_Con(AFFFD self, char *stream_name,
		    Properties props, FIFFile parent) {
  MapDriver this = (MapDriver)self;
  char *map;
  int length;
  char buff[BUFF_SIZE];

  // Call our baseclass
  self = this->__super__->Con(self, stream_name, props, parent);

  /** Try to load the map from the stream */
  snprintf(buff, BUFF_SIZE, "%s/map", stream_name);
  map = parent->super.read_member((ZipFile)parent, buff, &length);
  if(map) {
    // Make a temporary copy
    char *tmp = talloc_strdup(self, map);
    char *x=tmp, *y;
    struct map_point point;
    
    while(strlen(x)>0) {
      // Look for the end of line and null terminate it:
      y= x + strcspn(x, "\r\n");
      *y=0;
      if(sscanf(x,"%lld,%lld,%d", &point.file_offset, 
		&point.image_offset, &point.target_id)==3) {
	if(point.target_id < this->number_of_targets) 
	  CALL(this, add, point.file_offset, point.image_offset, 
	       (FileLikeObject)this->targets[point.target_id]);
      };
      x=y+1;
    };

  };

  return self;
};

static int compare_points(const void *X, const void *Y) {
  struct map_point *x=(struct map_point *)X;
  struct map_point *y=(struct map_point *)Y;

  return x->file_offset - y->file_offset;
};

void MapDriver_add(MapDriver self, uint64_t file_pos, uint64_t image_offset, 
		   FileLikeObject target) {
  int i,found=0;
  struct map_point new_point;
  MapDriver this=self;

  new_point.file_offset = file_pos;
  new_point.image_offset = image_offset;

  // Find the target number of the target (is it enough to just
  // compare pointers or do we need to compare the names?)
  for(i=0;i<this->number_of_targets;i++) {
    if((FileLikeObject)this->targets[i] == target) {
      found = 1;
      new_point.target_id = i;
      break;
    };
  };

  if(!found) {
    this->targets = talloc_realloc(self, this->targets, AFFFD, 
				   (this->number_of_targets+1) * sizeof(*this->targets));
    new_point.target_id = this->number_of_targets;
    this->targets[this->number_of_targets] = (AFFFD)target;
    this->number_of_targets++;
  };

  // Now append the new point to our struct:
  this->points = talloc_realloc(self, this->points, 
				struct map_point, 
				(this->number_of_points + 1) * sizeof(*this->points));
  memcpy(&this->points[this->number_of_points], &new_point, sizeof(new_point));

  // Now sort the array
  qsort(this->points, this->number_of_points, sizeof(*this->points),
	compare_points);
  this->number_of_points ++;
};

// This writes out the map to the stream
void MapDriver_save_map(MapDriver self) {
  char buff[BUFF_SIZE];
  FileLikeObject fd;
  struct map_point *point;
  int i;

  // Add the targets to our properties:
  for(i=0;i<self->number_of_targets;i++) {
    CALL(self->super.props, add, "target",
	 self->targets[i]->super.get_uri((FileLikeObject)self->targets[i]),0);
  };
  
  snprintf(buff, BUFF_SIZE, "%s/map", self->super.stream_name);
  fd = self->super.parent->super.open_member((ZipFile)self->super.parent,
					     buff, 'w',
					     NULL, 0, 0);
  for(i=0;i<self->number_of_points;i++) {
    point = &self->points[i];
    snprintf(buff, BUFF_SIZE, "%lld,%lld,%d\n", point->file_offset, point->image_offset, point->target_id);
    CALL(fd, write, ZSTRING_NO_NULL(buff));
  };

  CALL(fd, close);
};

void MapDriver_close(FileLikeObject self) {
  AFFFD pthis = (AFFFD)self;
  MapDriver this=(MapDriver)self;

  char tmp[BUFF_SIZE];

  // Set the stream size
  CALL(pthis->props, del, "size");
  snprintf(tmp, BUFF_SIZE, "%lld", self->size);
  CALL(pthis->props, add, "size", tmp, time(NULL)); 

  this->__super__->super.close(self);
};

#endif

VIRTUAL(MapDriver, AFFFD)
END_VIRTUAL

#if 0

VIRTUAL(MapDriver, AFFFD)
     VATTR(super.type) = "Map";

     VMETHOD(add) = MapDriver_add;
     VMETHOD(save_map) = MapDriver_save_map;
     VMETHOD(super.super.close) = MapDriver_close;
END_VIRTUAL


/* An array of handlers for different uri types. These handlers will
   be tried in order until one of them works, and returns the required
   object type 
*/
Object (*Resolver_handlers[])(Resolver self, char *uri);

/** This resolves the stream uri. We basically find out where it has
    been stored, and open it.
*/
Object resolve_AFF_Stream(Resolver self, char *uri) {
  char buff[BUFF_SIZE];
  char *file_reference;

  // Check where the stream might be stored
  snprintf(buff, BUFF_SIZE, "%s:aff2:storage", uri);
  file_reference = (char *)CALL(self->urn, get, buff);
  if(file_reference) {
    FIFFile fiffile = (FIFFile)CALL(self, resolve, file_reference, "FIFFile");

    if(fiffile) 
      return (Object)CALL(fiffile, open_stream, uri);
  };

  return NULL;
};

/** This method returns a fiffile referenced by the uri.

We first search the cache to see if the volume is already opened, if
not we see if we can find a volume and open it directly.
*/
Object resolve_AFF_Volumes(Resolver self, char *uri) {
  char buff[BUFF_SIZE];
  FIFFile fiffile;

  snprintf(buff, BUFF_SIZE, "%s:fiffile:cache", uri);
  fiffile = (FIFFile)CALL(self->urn, get, buff);

  // No its not in the cache, maybe we can just open the volume
  // directly:
  if(!fiffile) {
    FileLikeObject fd=NULL;
    char *reference;

    snprintf(buff, BUFF_SIZE, "%s:aff2:volume", uri);
    // Does the reference exit?
    reference = (char *)CALL(self->urn, get, buff);
    if(reference) {
      fd = (FileLikeObject)CALL(self, resolve, reference, "FileLikeObject");
      
      if(fd) {
	// Find the fiffile ourselves
	fiffile = CONSTRUCT(FIFFile, ZipFile, super.Con, self, fd);
      };
    };
  };
  
  return fiffile;
};

Object resolve_links(Resolver self, char *uri) {
  /* Is the uri a link? */
  char *target;
  char buff[BUFF_SIZE];

  snprintf(buff, BUFF_SIZE, "%s:aff2:link", uri);
  target = (char *)CALL(self->urn, get, buff);
  
  if(target) {
    printf("Resolved link as %s\n", target);
    return (Object)CALL(self, resolve, target, "FileLikeObject");
  };
  return NULL;
};


/** This handler tries to resolve a file://filename reference by
    opening the said file in the current directory.
*/
Object file_handler(Resolver self, char *uri) {
  FileLikeObject fd;

  if(!memcmp(uri, ZSTRING_NO_NULL("file://"))) {
    return (Object)CONSTRUCT(FileBackedObject, FileBackedObject, Con, self, 
			     uri + strlen("file://"), 'r');
  };
  
  return NULL;
};

#endif

VIRTUAL(Link, AFFObject)
END_VIRTUAL
