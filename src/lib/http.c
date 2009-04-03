#include "zip.h"

/******************************************************************
   This is an implementation of a simple HTTP File Like Object.

We basically use libcurl to do the heavy lifting so we might actually
get to use ftp://, http://, https:// and more all for free.

******************************************************************/
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *self){ 
  HTTPObject this = (HTTPObject)self;

  CALL(this->buffer, write, ptr, size*nmemb);
  return size*nmemb;
};

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *self){ 
  HTTPObject this = (HTTPObject)self;

  int len;
  CALL(this->send_buffer, seek, 0, 0);

  len = CALL(this->send_buffer, read, ptr, size*nmemb);
  CALL(this->send_buffer, skip, len);
  CALL(this->send_buffer, seek, 0, 2);
  //  if(len==0) return CURL_READFUNC_PAUSE;
  return len;
};

// We want to get this header, Content-Range: bytes 0-100/3388
static size_t parse_header_callback(void *ptr, size_t size, size_t nmemb, void *self) {
  HTTPObject this = (HTTPObject)self;
  int length = size * nmemb;
  char *string;
  char *x;
  static char *content_range = "Content-Range: bytes ";
  uint64_t tmp,tmp_size;

  string = talloc_size(self, length +1);
  // Null terminate the string
  memcpy(string, ptr, length);
  x=strstr(string, content_range);
  if(x && x==string &&
     3 == sscanf(string + strlen(content_range), "%lld-%lld/%lld", 
		 &tmp, &tmp, &tmp_size)) {
    ((FileLikeObject)this)->size = tmp_size;
  };

  talloc_free(string);
  return length;
};

static AFFObject HTTPObject_AFFObject_Con(AFFObject self, char *uri) {
  HTTPObject this = (HTTPObject)self;

  return (AFFObject)CALL(this, Con, uri);
};


static HTTPObject HTTPObject_Con(HTTPObject self,
				 char *url) {
  // Parse out the url
  CURL *handle = self->curl = curl_easy_init();

  self->buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  URNOF(self) = talloc_strdup(self, url);

  //  curl_easy_setopt( handle, CURLOPT_VERBOSE, 1L ); 
  curl_easy_setopt( handle, CURLOPT_URL, url);

  curl_easy_setopt( handle, CURLOPT_WRITEFUNCTION, write_callback );
  curl_easy_setopt( handle, CURLOPT_WRITEDATA, self );
  curl_easy_setopt( handle, CURLOPT_HEADERFUNCTION, parse_header_callback);
  curl_easy_setopt( handle, CURLOPT_HEADERDATA, self);

  if(!self->curl) {
    RaiseError(ERuntimeError, "Unable to initialise curl");
    goto error;
  };

  // Read 1 byte to work out the file size:
  { 
    char buffer[10];
    CALL((FileLikeObject)self, read, buffer, 1);
  };

  return self;
 error:
  talloc_free(self);
  return NULL;
};

static int HTTPObject_read(FileLikeObject self, char *buffer,
			   unsigned long int length) {
  HTTPObject this = (HTTPObject)self;
  char range[BUFF_SIZE];

  CALL(this->buffer, truncate,0 );
  // Tell the server we only want a range
  snprintf(range, BUFF_SIZE, "%lld-%lld", self->readptr, 
	   self->readptr + length);
  curl_easy_setopt( this->curl, CURLOPT_RANGE, range ); 
  curl_easy_perform( this->curl ); 

  length = min(length, this->buffer->readptr);
  memcpy(buffer, this->buffer->data, length);

  self->readptr+=length;
  return length;
};

static int HTTPObject_write(FileLikeObject self, char *buffer,
			    unsigned long int length) {
  HTTPObject this = (HTTPObject)self;

  if(!this->send_handle) {
    CURL *send_handle = this->send_handle = curl_easy_init();
    this->multi_handle = curl_multi_init();

    this->send_buffer = CONSTRUCT(StringIO, StringIO, Con, self);

    /** Set up the upload handle */
    curl_easy_setopt( send_handle, CURLOPT_URL, URNOF(self));
    curl_easy_setopt( send_handle, CURLOPT_INFILESIZE_LARGE, -1);
    curl_easy_setopt( send_handle, CURLOPT_UPLOAD, 1);
    curl_easy_setopt( send_handle, CURLOPT_READFUNCTION, read_callback );
    curl_easy_setopt( send_handle, CURLOPT_READDATA, self );  

    // Ok tell libcurl to upload the buffer. Because this has no
    // data we expect it to pause right away.
    curl_multi_add_handle(this->multi_handle, this->send_handle);
  };

  // Write the buffer on the end
  CALL(this->send_buffer, seek, 0, SEEK_END);
  CALL(this->send_buffer, write, buffer, length);
  // Unpause the connection
  {
    int handle_count;
    while(curl_multi_perform( this->multi_handle, &handle_count)==CURLM_CALL_MULTI_PERFORM);
  };

  self->readptr += length;
  self->size = max(self->size, self->readptr);

  return length;
};

void HTTPObject_close(FileLikeObject self) {
  HTTPObject this = (HTTPObject)self;

  // Call our base class
  this->__super__->close(self);

  curl_easy_cleanup(this->curl);

  if(this->send_handle) {
    int handle_count;

    while(this->send_buffer->size > 0) {
      int handle_count;
      int result = curl_multi_perform(this->multi_handle, &handle_count);
    };

    curl_multi_remove_handle(this->multi_handle, this->send_handle);
    curl_easy_cleanup(this->send_handle);
    curl_multi_cleanup(this->multi_handle);
    this->send_handle = NULL;
    this->multi_handle = NULL;
  };
};

VIRTUAL(HTTPObject, FileLikeObject)
     VMETHOD(super.read) = HTTPObject_read;
     VMETHOD(super.write) = HTTPObject_write;
     VMETHOD(super.close) = HTTPObject_close;
     VMETHOD(Con) = HTTPObject_Con;
     VMETHOD(super.super.Con) = HTTPObject_AFFObject_Con;
// This should only be called once:
     curl_global_init( CURL_GLOBAL_ALL ); 
END_VIRTUAL
