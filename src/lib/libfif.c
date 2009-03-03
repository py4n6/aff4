#include "fif.h"
#include "time.h"
#include <uuid/uuid.h>


/** This is a dispatcher of stream classes depending on their name */
struct dispatch_t dispatch[] = {
  { "Image", (AFFFD)(&__Image) },
  { "Map", (AFFFD)(&__MapDriver)},
  { NULL, NULL}
};

/** We need to call these initialisers explicitely or the class
    references wont work.
*/
static void init_streams(void) {
  FileLikeObject_init();
  AFFFD_init();
  Image_init();
  MapDriver_init();
};

char *new_uuid(void *ctx) {
  uuid_t uuid;
  char *uuid_str;

  // This function creates a new stream from scratch so the stream
  // name will be a new UUID
  uuid_generate(uuid);
  uuid_str = talloc_size(ctx, 40);
  uuid_unparse(uuid, uuid_str);

  return uuid_str;
};

/** Dispatch and construct the stream from its type. Stream will be    
    opened for writing. Callers should repeatadly write on it and must
    call close() before a new stream can be opened.
**/
static AFFFD FIFFile_create_stream_for_writing(FIFFile self, char *stream_name,
					char *stream_type, Properties props) {
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

  RaiseError(ERuntimeError, "No handler for stream_type '%s'", stream_type);
  return NULL;
};

static void FIFFile_create_new_volume(ZipFile self, FileLikeObject file) {
  FIFFile this=(FIFFile)self;
  char buffer[BUFF_SIZE];
  char *fd_uri = CALL(file,get_uri);

  // We are about to open a new volume - record that
  snprintf(buffer, BUFF_SIZE, "urn:aff2:%s:aff2:storage", this->uuid);
  CALL(this->resolver, add, buffer, ZSTRING_NO_NULL(fd_uri));
  
  this->__super__->create_new_volume(self, file);
};

static AFFFD FIFFile_open_stream(FIFFile self, char *stream_name) {
  int length=0;
  char *buffer;
  Properties props;
  char *stream_type;
  int i;
  char *path = talloc_asprintf(self, "%s/properties", stream_name);

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

  // Some initialization housework
  init_streams();

  // This is our resolver
  this->resolver = CONSTRUCT(Resolver, Resolver, Con, self);
  this->props = CONSTRUCT(Properties, Properties, Con, self);

  self = this->__super__->Con(self, fd);
  if(!self) return NULL;

  // If an FD was provided we parse its properties
  if(fd) {
    int length;
    char *properties_file = CALL(self, read_member, "properties", &length);

    if(properties_file) {
      // Parse our properties
      CALL(this->props, parse, properties_file, length, 0);
    };

    // Try to get the uuid of this volume
    this->uuid = CALL(this->props, iter_next, NULL, "storage:UUID");
    
  } else {
    // Otherwise we prepare a fresh properties object
    this->props = CONSTRUCT(Properties, Properties, Con, self);
  };

  // Close us if we get freed 
  talloc_set_destructor(self, FIFFile_destructor);

  if(!this->uuid) {
    // Make up a new urn for this volume
    this->uuid = new_uuid(self);
    CALL(this->props, add, "storage:UUID", this->uuid, 0);
  };

  // Stores this in the resolver
  if(fd) {
    char buffer[BUFF_SIZE];
    char *fd_uri = CALL(fd, get_uri);
    Properties i=NULL;

    // Make up a URN to record where we got this volume
    snprintf(buffer, BUFF_SIZE, "urn:aff2:%s:aff2:storage",this->uuid);

    // Cache this:
    CALL(this->resolver, add, buffer, ZSTRING_NO_NULL(fd_uri));

    // Now advise the resolver where the streams are
    while(1) {
      char *urn = CALL(this->props, iter_next, &i, "aff2:contains");
      
      if(!urn) 
	break;
      else {
	char *our_urn = talloc_asprintf(self, "urn:aff2:%s", this->uuid);
	
	snprintf(buffer, BUFF_SIZE, "%s:aff2:volume", urn);
	CALL(this->resolver, add, buffer, ZSTRING_NO_NULL(our_urn));
      };
    };
  };

  return self;
};

static void FIFFile_add_zipinfo_to_cache(ZipFile self, ZipInfo zip) {
  FIFFile this = (FIFFile)self;
  int filename_length=strlen(zip->filename);
  int properties_length = strlen("properties");

  // We identify streams by their filename ending with "properties"
  if(filename_length > properties_length && 
     !strcmp("properties", zip->filename + filename_length - properties_length)) {
    char *uuid = talloc_strdup(self, zip->filename);

    // This is our current FIFFile uuid
    char *our_urn = talloc_asprintf(self, "urn:aff2:%s", this->uuid);
    char buffer[BUFF_SIZE];

    uuid[filename_length-properties_length-1]=0;

    // Add a reference to our global properties file to denote that we
    // contain these streams.
    CALL(this->props, add, "aff2:contains", 
	 talloc_asprintf(self, "urn:aff2:%s",uuid),0);
  };
  printf("Found %s\n", zip->filename);

  // Call our baseclass
  this->__super__->add_zipinfo_to_cache(self,zip);

};

static void FIFFile_close(ZipFile self) {
  // Force our properties to be written
  FIFFile this = (FIFFile)self;
  char *str;
  if(self->_didModify) {
    str = CALL(this->props, str);
    CALL(self, writestr, "properties", ZSTRING_NO_NULL(str),
	 NULL,0, 0);
  };

  // Call our base class:
  this->__super__->close(self);
};

Object FIFFile_resolve(FIFFile self, char *urn, char *type) {
  char *fqn;
  Object result;

  //Is this a local reference or a global reference?
  if(strlen(urn)>8 && !memcmp(urn,ZSTRING_NO_NULL("urn:aff2:"))) {
    fqn = talloc_strdup(self, urn);
  } else {
    fqn = talloc_asprintf(self, "urn:aff2:%s:%s", self->uuid, urn);
  };

  result = CALL(self->resolver, resolve, fqn, type);

  talloc_free(fqn);
  return result;
};


VIRTUAL(FIFFile, ZipFile)
     VMETHOD(create_stream_for_writing) = FIFFile_create_stream_for_writing;
     VMETHOD(open_stream)  = FIFFile_open_stream;
     VMETHOD(super.close) = FIFFile_close;
     VMETHOD(super.Con) = FIFFile_Con;
     VMETHOD(super.add_zipinfo_to_cache) = FIFFile_add_zipinfo_to_cache;
     VMETHOD(super.create_new_volume) = FIFFile_create_new_volume;
VMETHOD(resolve) = FIFFile_resolve;
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
     VMETHOD(Con) = AFFFD_Con;
     VMETHOD(super.close) = AFFFD_close;
     VMETHOD(super.get_uri) = AFFFD_get_uri;
END_VIRTUAL

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

static AFFFD Image_Con(AFFFD self, char *stream_name, Properties props, FIFFile parent) {
  Image this = (Image)self;
  char *tmp;

  // Call our baseclass
  this->__super__->Con(self, stream_name, props, parent);

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

VIRTUAL(Image, AFFFD)
     VMETHOD(super.Con) = Image_Con;
     VMETHOD(super.super.write) = Image_write;
     VMETHOD(super.super.read) = Image_read;
     VMETHOD(super.super.close) = Image_close;

     VATTR(super.type) = "Image";
END_VIRTUAL


/*** This is the implementation of properties */
static Properties Properties_Con(Properties self) {
  INIT_LIST_HEAD(&self->list);

  return self;
};

static void Properties_del(Properties self, char *key) {
  Properties i,j;
  
  list_for_each_entry_safe(i, j, &self->list, list) {
    if(!strcmp(i->key, key)) {
      list_del(&i->list);
      talloc_free(i);
    };
  };
};

static char *Properties_add(Properties self, char *key, char *value,
		    uint32_t date) {
  Properties new;
  Properties tmp;

  // Ignore this if the key,value is already in there.
  list_for_each_entry(tmp, &self->list, list) {
    if(tmp->key && !strcmp(tmp->key, key) && !strcmp(tmp->value, value)) {
      return value;
    };
  };

  new = CONSTRUCT(Properties, Properties, Con, self);
  new->key = talloc_strdup(new, key);
  new->value = talloc_strdup(new, value);
  new->date = date;

  list_add_tail(&new->list, &self->list);

  return value;
};

static char *Properties_iter_next(Properties self, Properties *current, char *key) {
  Properties result;

  if(!current) 
    current = &self;
  else if(*current == self) 
    return NULL;
  else if(*current==NULL) {
    *current = self;
  };
  
  // Find the next property that matches the key
  list_for_each_entry(result, &(*current)->list, list) {
    if(result == self) break;
    if(!strcmp(key, result->key)) {
      *current = result;
      return result->value;
    };
  }

  return NULL;
};

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

VIRTUAL(Properties, Object)
     VMETHOD(Con) = Properties_Con;
     VMETHOD(add) = Properties_add;
     VMETHOD(parse) = Properties_parse;
     VMETHOD(iter_next) = Properties_iter_next;
     VMETHOD(str) = Properties_str;
     VMETHOD(del) = Properties_del;
END_VIRTUAL

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
    return (Object)CONSTRUCT(FileBackedObject, FileBackedObject, con, self, 
			     uri + strlen("file://"), 'r');
  };
  
  return NULL;
};

static Resolver Resolver_Con(Resolver self) {
  return self;
};

Object Resolver_resolve(Resolver self, char *urn, char *class_name) {
  Object (**handler)(Resolver self, char *urn) = Resolver_handlers;
  Object result;
  int i;

  for(i=0;handler[i];i++) {
    result = handler[i](self, urn);
    if(result) return result;
  };
  
  return NULL;
};

void Resolver_add(Resolver self, char *uri, void *value, int length) {
  char *uri_str = talloc_strdup(self, uri);
 
  printf("Adding to resolver: %s=%s\n", uri, (char *)value);
  fflush(stdout);
  CALL(__Resolver.urn, put, uri_str, value, length);
};

/** Here we implement the resolver */
VIRTUAL(Resolver, Object)
     VMETHOD(Con) = Resolver_Con;
/* The cache is a class attribute so all instances use a singleton
 cache. We never expire this cache. The cache contains global
 attributes in urn notation. Its basically a dictionary with urn
 attributes as key and the attribute values as values. URNs may be
 resolved via an external service as well. Currently supported is a
 HTTP resolver which can be set by the AFF2_HTTP_RESOLVER environment
 variable.
*/
     __Resolver.urn = CONSTRUCT(Cache, Cache, Con, NULL, HASH_TABLE_SIZE, 1e6);
     VMETHOD(resolve) = Resolver_resolve;
     VMETHOD(add)  = Resolver_add;
END_VIRTUAL

/* Initialise the resolver handler dispatch table. This is a table of
 hander dispatchers which will be tried one at the time to resolve the
 provided uri into an object. URIs can refer to any object at
 all. This array is used internally by the Resolver and its probably
 not that useful outside of it.
*/
Object (*Resolver_handlers[])(Resolver self, char *uri) = { 
  file_handler, resolve_AFF_Volumes, resolve_AFF_Stream, 
  resolve_links,
  NULL};
