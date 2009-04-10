#include "zip.h"

static char CURL_ERROR[CURL_ERROR_SIZE];

/******************************************************************
   This is an implementation of a simple HTTP File Like Object.

We basically use libcurl to do the heavy lifting so we might actually
get to use ftp://, http://, https:// and more all for free.

    You can always get the curl options required:

    curl --libcurl /tmp/test.c -X MKCOL http://localhost/webdav/test/

This implementation uses webdav to write the image on the server as
needed. You can use a Zip volume or a directory volume as needed. The
following is an example of how to set up apache to support
webdav. Basically add this to the default host configuration file:

<Directory "/var/www/webdav/" >
     DAV On
     AuthType Basic
     AuthName "test"
     AuthUserFile /etc/apache2/passwd.dav

     <Limit PUT POST DELETE PROPPATCH MKCOL COPY BCOPY MOVE LOCK \
     UNLOCK>
        Allow from 127.0.0.0/255.0.0.0
        Require user mic
        Satisfy Any
     </Limit>
</Directory>

This allows all access from 127.0.0.1 but requires an authenticated
user to modify things from anywhere else. Read only access is allowed
from anywhere.

******************************************************************/
// A do nothing write callback to ignore the body
static size_t null_write_callback(void *ptr, size_t size, size_t nmemb, void *self){ 
  return size*nmemb;
};

// We use this to write to our own buffers
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *self){ 
  HTTPObject this = (HTTPObject)self;

  CALL(this->buffer, write, ptr, size*nmemb);
  return size*nmemb;
};

/** Read as much as we can from the send buffer and rotate it along */
static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *self){ 
  HTTPObject this = (HTTPObject)self;

  int len;
  CALL(this->send_buffer, seek, 0, 0);

  len = CALL(this->send_buffer, read, ptr, size*nmemb);
  CALL(this->send_buffer, skip, len);
  CALL(this->send_buffer, seek, 0, 2);

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

/** This function ensures that the directories all exist up to the
    current directory.
*/
static int webdav_recurse_dir_check(HTTPObject self, char *url) {
  char buff[BUFF_SIZE];
  int len;
  int res;
  strncpy(buff, url, BUFF_SIZE);

  dirname(buff);
  len = strlen(buff);
  buff[len]='/';
  buff[len+1] =0;
  if(len < 10) return -1;

  // Check that the directory exists
  curl_easy_setopt(self->send_handle, CURLOPT_URL, buff);
  curl_easy_setopt(self->send_handle, CURLOPT_CUSTOMREQUEST, "PROPFIND");
  res = curl_easy_perform(self->send_handle);
  if(res != 0) {
    // The present directory does not exist - make sure the parent
    // directory exists, and then create the current directory within
    // it:
    webdav_recurse_dir_check(self, buff);
    curl_easy_setopt(self->send_handle, CURLOPT_URL, buff);
    curl_easy_setopt(self->send_handle, CURLOPT_CUSTOMREQUEST, "MKCOL");
    curl_easy_perform(self->send_handle);
  };
};

static AFFObject HTTPObject_AFFObject_Con(AFFObject self, char *uri) {
  HTTPObject this = (HTTPObject)self;

  return (AFFObject)CALL(this, Con, uri);
};


static HTTPObject HTTPObject_Con(HTTPObject self,
				 char *url) {
  // Parse out the url
  CURL *handle = self->curl = curl_easy_init();

  if(!self->curl) {
    RaiseError(ERuntimeError, "Unable to initialise curl");
    goto error;
  };

  self->buffer = CONSTRUCT(StringIO, StringIO, Con, self);
  URNOF(self) = talloc_strdup(self, url);

  curl_easy_setopt( handle, CURLOPT_VERBOSE, 0L); 
  curl_easy_setopt( handle, CURLOPT_URL, url);
  curl_easy_setopt( handle, CURLOPT_FAILONERROR , 1L);
  curl_easy_setopt( handle, CURLOPT_ERRORBUFFER , CURL_ERROR);

  curl_easy_setopt( handle, CURLOPT_WRITEFUNCTION, write_callback );
  curl_easy_setopt( handle, CURLOPT_WRITEDATA, self );
  curl_easy_setopt( handle, CURLOPT_HEADERFUNCTION, parse_header_callback);
  curl_easy_setopt( handle, CURLOPT_HEADERDATA, self);

  // Read 1 byte to work out the file size through the write callback:
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

  CALL(this->buffer, truncate, 0);
  // Tell the server we only want a range
  snprintf(range, BUFF_SIZE, "%lld-%lld", self->readptr, 
	   self->readptr + length);
  curl_easy_setopt( this->curl, CURLOPT_RANGE, range ); 
  if(curl_easy_perform( this->curl ) != 0) {
    // Error occured:
    return -1;
  };

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

    // Make sure the present directory exists on the server:
    curl_easy_setopt( send_handle, CURLOPT_ERRORBUFFER , CURL_ERROR);
    curl_easy_setopt( send_handle, CURLOPT_FAILONERROR , 1L);
    curl_easy_setopt( send_handle, CURLOPT_WRITEFUNCTION, null_write_callback );
    webdav_recurse_dir_check(this, URNOF(self));

    /** Set up the upload handle */
    curl_easy_reset( send_handle);
    curl_easy_setopt( send_handle, CURLOPT_WRITEFUNCTION, null_write_callback );
    curl_easy_setopt( send_handle, CURLOPT_VERBOSE, 0L); 
    curl_easy_setopt( send_handle, CURLOPT_ERRORBUFFER , CURL_ERROR);
    curl_easy_setopt( send_handle, CURLOPT_FAILONERROR , 1L);
    curl_easy_setopt( send_handle, CURLOPT_URL, URNOF(self));
    curl_easy_setopt( send_handle, CURLOPT_INFILESIZE_LARGE, -1);
    curl_easy_setopt( send_handle, CURLOPT_UPLOAD, 1);
    curl_easy_setopt( send_handle, CURLOPT_READFUNCTION, read_callback );
    curl_easy_setopt( send_handle, CURLOPT_READDATA, self );  

    // Uploads are done via the multi interface:
    curl_multi_add_handle(this->multi_handle, this->send_handle);
  };

  // Write the buffer on the end
  CALL(this->send_buffer, seek, 0, SEEK_END);
  CALL(this->send_buffer, write, buffer, length);
  // Send as much as we can right now:
  {
    int handle_count;
    while(curl_multi_perform( this->multi_handle, &handle_count)==CURLM_CALL_MULTI_PERFORM);
  };

  self->readptr += length;
  self->size = max(self->size, self->readptr);

  return length;
};

static void HTTPObject_close(FileLikeObject self) {
  HTTPObject this = (HTTPObject)self;

  // Call our base class
  this->__super__->close(self);

  curl_easy_cleanup(this->curl);

  if(this->send_handle) {
    int handle_count;

    while(this->send_buffer->size > 0) {
      int handle_count;
      int result = curl_multi_perform(this->multi_handle, &handle_count);
      // An error occured
      if(!handle_count || result !=0) {
	RaiseError(ERuntimeError, "Curl error: %s", CURL_ERROR);
	break;
      };
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
