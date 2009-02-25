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

VIRTUAL(FIFFile, ZipFile)
     VMETHOD(create_stream_for_writing) = FIFFile_create_stream_for_writing;
END_VIRTUAL

AFFFD AFFFD_Con(AFFFD self, char *stream_name, Properties props, FIFFile parent) {
  return self;
};

VIRTUAL(AFFFD, FileLikeObject)
     VMETHOD(Con) = AFFFD_Con;
END_VIRTUAL

AFFFD Image_Con(AFFFD self, char *stream_name, Properties props, FIFFile parent) {
  Image this = (Image)self;
  char *tmp;

  printf("Constructing an image %s\n", stream_name);
  if(!props) props = CONSTRUCT(Properties, Properties, Con, self);
  
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
  self->parent = parent;

  this->chunk_indexes = talloc_array(self, int32_t, this->chunks_in_segment);
  // Fill it with -1 to indicate an invalid pointer
  memset(this->chunk_indexes, -1, sizeof(int32_t) * this->chunks_in_segment);

  return self;
};

/** This is how we implement the Image stream writer:

As new data is written we append it to the chunk buffer, then we
remove chunk sized buffers from it and push them to the segment. When
the segment is full we dump it to the parent container set.
**/
int dump_chunk(Image this, char *data, uint32_t length, int force) {
    // We just use compress() to get the compressed buffer.
    char buffer[length + 1024];
    int clength=length + 1024;

    if(compress((unsigned char *)buffer, (unsigned int )&clength, 
		(unsigned char *)data, (unsigned int)length)!=Z_OK) {
      RaiseError(ERuntimeError, "Compression error");
      return -1;
    };

    // Update the index to point at the current segment stream buffer
    // offset
    this->chunk_indexes[this->chunk_count] = this->segment_buffer->readptr;

    // Now write the buffer to the stream buffer
    CALL(this->segment_buffer, write, buffer, clength);
    this->chunk_count ++;

    // Is the segment buffer full? If it has enough chunks in it we
    // can flush it out:
    if(this->chunk_count >= this->chunks_in_segment || force) {
      char tmp[BUFF_SIZE];
      snprintf(tmp, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, this->segment_count);
      this->super.parent->super.writestr((ZipFile)this->super.parent, 
					 tmp, this->segment_buffer->data, 
					 this->segment_buffer->size, 
					 // No compression for segments
					 ZIP_STORED);

      // Reset everything to the start
      CALL(this->segment_buffer, truncate, 0);
      memset(this->chunk_indexes, -1, sizeof(int32_t) * this->chunks_in_segment);
      this->chunk_count =0;

      this->segment_count ++;
    };
};

int Image_write(FileLikeObject self, char *buffer, int length) {
  Image this = (Image)self;

  CALL(this->chunk_buffer, write, buffer, length);
  while(this->chunk_buffer->size > this->chunk_size) {
    if(dump_chunk(this, this->chunk_buffer->data, this->chunk_size, 0)<0)
      return -1;
    // Consume the chunk from the chunk_buffer
    CALL(this->chunk_buffer, skip, this->chunk_size);
  };

  return length;
};

void Image_close(FileLikeObject self) {
  Image this = (Image)self;

  // Write the last chunk
  dump_chunk(this, this->chunk_buffer->data, this->chunk_buffer->size, 1);
};

/** Reads at most a single chunk and write to result. Return how much
    data was actually read.
*/
static int partial_read(FileLikeObject self, StringIO result, int length) {
  // which segment is it?
  int chunk_id = self->readptr / this->chunk_size;
  int segment_id = chunk_id / this->chunks_in_segment;

  int chunk_offset = self->readptr % this->chunksize;
  int available_to_read = min(this->chunksize - chunk_offset, length);
  char *chunk;
  
  /** Now we need to figure out where the segment is */
  char filename[BUFF_SIZE];
  ZipInfo zip;

  snprintf(filename, BUFF_SIZE, IMAGE_SEGMENT_NAME_FORMAT, segment_id);
  zip = CALL(self->parent, fetch_ZipInfo, filename);

  // Just interpolate NULLs if the segment is missing
  if(!zip) {
    char pad[available_to_read];

    memset(pad,0, this->chunksize);
    RaiseError(ERuntimeError, "Segment %s for offset %lld is missing, padding", 
	       filename, self->readptr);
    CALL(result, write, pad, available_to_read);
  } else {
    // Fetch the extra field from the zip file header - this is the
    // chunk index into the segment: 
    CALL(self->fd, seek, zip->cd_header.relative_offset_local_header,
	 SEEK_SET);
    if(CALL(self->fd, read,(char *)&header, read_length) != read_length)
      goto error;

    
  chunk = self.read_chunk(chunk_id);

  length = CALL(result, write, chunk + chunk_offset, available_to_read);

  talloc_free(chunk);

  return length;
};

// Reads from the image stream
int Image_read(FileLikeObject self, char *buffer, int length) {


};


VIRTUAL(Image, AFFFD)
     VMETHOD(super.Con) = Image_Con;
     VMETHOD(super.super.write) = Image_write;
     VMETHOD(super.super.close) = Image_close;
END_VIRTUAL


/*** This is the implementation of properties */
Properties Properties_Con(Properties self) {
  INIT_LIST_HEAD(&self->list);

  return self;
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
    current = &result;
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
int Properties_parse(Properties self, char *text, uint32_t date) {
  int i,j;
  // Make our own local copy so we can modify it
  char *tmp = talloc_strdup(self, text);
  char *tmp_text = tmp;

  // Find the next line:
  while((i=strcspn(tmp_text, "\r\n"))) {
    tmp_text[i]=0;

    j=strcspn(tmp_text,"=");
    if(j==i) goto exit;
    
    tmp_text[j]=0;
    CALL(self, add, tmp_text, tmp_text+j+1, date);
    tmp_text += i;
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
  
  return result;
};

VIRTUAL(Properties, Object)
     VMETHOD(Con) = Properties_Con;
     VMETHOD(add) = Properties_add;
     VMETHOD(parse) = Properties_parse;
     VMETHOD(iter_next) = Properties_iter_next;
     VMETHOD(str) = Properties_str;
END_VIRTUAL
