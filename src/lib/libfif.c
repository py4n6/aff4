#include "fif.h"
#include "time.h"

/** This is a dispatches of stream classes depending on their name */
struct dispatch_t dispatch[] = {
  { "Image", (AFFFD)(&__Image) },
  { NULL, NULL}
};

/** We need to call these initialisers explicitely or the class
    references wont work.
*/
static void init_streams(void) {
  FileLikeObject_init();
  AFFFD_init();
  Image_init();
};

/** Dispatch and construct the stream from its type. Stream will be
    
    opened for writing. Callers should repeatadly write on it and must
    call close() before a new stream can be opened.
**/
AFFFD FIFFile_create_stream_for_writing(FIFFile self, char *stream_name,
					char *stream_type, Properties props) {
  int i;

  init_streams();

  for(i=0; dispatch[i].type !=NULL; i++) {
    if(!strcmp(dispatch[i].type, stream_type)) {
      AFFFD result;

      // A special constructor from a class reference
      result = CONSTRUCT_FROM_REFERENCE(dispatch[i].class_ptr, 
					Con, self, stream_name, props, self);
      return result;
    };
  };

  RaiseError(ERuntimeError, "No handler for stream_type '%s'", stream_type);
  return NULL;
};

AFFFD FIFFile_open_stream(FIFFile self, char *stream_name) {
  int length=0;
  char *buffer;
  Properties props;
  char *stream_type;
  int i;

  init_streams();

  props = CONSTRUCT(Properties, Properties, Con, self);
  buffer = self->super.read_member((ZipFile)self, "properties", &length);
  if(!buffer) {
    RaiseError(ERuntimeError, "Stream %s does not have properties?", stream_name);
    talloc_free(buffer);
    return NULL;
  };
  
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

VIRTUAL(FIFFile, ZipFile)
     VMETHOD(create_stream_for_writing) = FIFFile_create_stream_for_writing;
     VMETHOD(open_stream)  = FIFFile_open_stream;
END_VIRTUAL

AFFFD AFFFD_Con(AFFFD self, char *stream_name, Properties props, FIFFile parent) {
  return self;
};

VIRTUAL(AFFFD, FileLikeObject)
     VMETHOD(Con) = AFFFD_Con;
END_VIRTUAL

/** This specialised hashing is for integer keys */
unsigned int cache_hash_int(Cache self, void *key) {
  uint32_t int_key = *(uint32_t *)key;

  return int_key % self->hash_table_width;
};

int cache_cmp_int(Cache self, void *other) {
  uint32_t int_key = *(uint32_t *)self->key;
  uint32_t int_other = *(uint32_t *)other;  

  return int_key != int_other;
};

AFFFD Image_Con(AFFFD self, char *stream_name, Properties props, FIFFile parent) {
  Image this = (Image)self;
  char *tmp;

  printf("Constructing an image %s\n", stream_name);
  if(!props) {
    props = CONSTRUCT(Properties, Properties, Con, self);
  } else{
    talloc_steal(this, props);
  };

  self->props = props;

  // Ensure that we have the type in there
  CALL(props, del, "type");
  CALL(props, add, "type", "Image", time(NULL));

  // Set the defaults
  tmp = CALL(props, iter_next, NULL, "chunks_in_segment");
  if(!tmp) tmp = CALL(props, add, "chunks_in_segment", "2048", time(NULL));
  this->chunks_in_segment = strtol( tmp, NULL, 0);

  tmp = CALL(props, iter_next, NULL, "chunk_size");
  if(!tmp) tmp = CALL(props, add, "chunk_size", "0x8000", time(NULL));

  this->chunk_size = strtol(tmp, NULL,0);

  tmp = CALL(props, iter_next, NULL, "size");
  if(tmp)
    self->super.size = strtoll(tmp, NULL, 0);
  
  this->chunk_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  this->segment_buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  this->chunk_count = 0;
  this->segment_count =0;
  self->parent = parent;

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
int dump_chunk(Image this, char *data, uint32_t length, int force) {
  // We just use compress() to get the compressed buffer.
  char cbuffer[compressBound(length)];
  int clength=compressBound(length);

  memset(cbuffer,0,clength);

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
    snprintf(tmp, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, this->segment_count);
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
    
    this->segment_count ++;
  };
  
  return clength;
};

int Image_write(FileLikeObject self, char *buffer, unsigned long int length) {
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

void Image_close(FileLikeObject self) {
  Image this = (Image)self;
  char *properties_str;
  AFFFD pthis = (AFFFD)self;
  char tmp[BUFF_SIZE];

  // Write the last chunk
  dump_chunk(this, this->chunk_buffer->data, this->chunk_buffer->readptr, 1);

  // Set the stream size
  CALL(pthis->props, del, "size");
  snprintf(tmp, BUFF_SIZE, "%lld", self->size);
  CALL(pthis->props, add, "size", tmp, time(NULL)); 

  // Write a properties file
  properties_str = CALL(pthis->props, str);
  pthis->parent->super.writestr((ZipFile)pthis->parent,
				"properties", ZSTRING_NO_NULL(properties_str), NULL, 0, 0);
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
  uncompressed_chunk = talloc_size(self, uncompressed_length);

  // What is the segment name?
  filename = talloc_asprintf(self, IMAGE_SEGMENT_NAME_FORMAT, segment_id);

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
  CALL(this->chunk_cache, put, talloc_memdup(self, &chunk_id, sizeof(chunk_id)),
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
int Image_read(FileLikeObject self, char *buffer, unsigned long int length) {
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
END_VIRTUAL


/*** This is the implementation of properties */
Properties Properties_Con(Properties self) {
  INIT_LIST_HEAD(&self->list);

  return self;
};

void Properties_del(Properties self, char *key) {
  Properties i,j;
  
  list_for_each_entry_safe(i, j, &self->list, list) {
    if(!strcmp(i->key, key)) {
      list_del(&i->list);
      talloc_free(i);
    };
  };
};

char *Properties_add(Properties self, char *key, char *value,
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

char *Properties_iter_next(Properties self, Properties *current, char *key) {
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
int Properties_parse(Properties self, char *text, uint32_t len, uint32_t date) {
  int i,j;
  // Make our own local copy so we can modify it
  char *tmp = talloc_memdup(self, text, len+1);
  char *tmp_text = tmp;

  tmp[len]=0;

  // Find the next line:
  while((i=strcspn(tmp_text, "\r\n"))) {
    tmp_text[i]=0;

    j=strcspn(tmp_text,"=");
    if(j==i) goto exit;
    
    tmp_text[j]=0;
    CALL(self, add, tmp_text, tmp_text+j+1, date);
    tmp_text += i+1;
  };

 exit:
  talloc_free(tmp);
  return 0;
};

char *Properties_str(Properties self) {
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
