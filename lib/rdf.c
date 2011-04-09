/** This handles the RDF serialization, and parsing from the
    resolver.
*/
/** The following are related to raptor - RDF serialization and
    parsing
*/
#include "misc.h"

/** Parsing URLs - basically a state machine */
enum url_state {
  STATE_METHOD,
  STATE_COLLON,
  STATE_NETLOCK,
  STATE_QUERY,
  STATE_FRAGMENT,
  STATE_END
};

static URLParse URLParse_Con(URLParse self, char *url) {
  if(url)
    CALL(self, parse ,url);
  self->ctx = talloc_size(self, 1);
  return self;
};

static int URLParse_parse(URLParse self, char *url) {
  int i=0,j=0,end=0;
  char buff[BUFF_SIZE];
  enum url_state state = STATE_METHOD;

  talloc_free(self->ctx);
  self->ctx = talloc_size(self, 1);

  // Initialise with defaults:
  self->scheme = "";
  self->netloc = "";
  self->query = "";
  self->fragment = "";

  // Its ok to call us with NULL - we just wont parse anything.
  if(!url) goto exit;

  while(!end) {
    if(url[i]==0) end=1;

    switch(state) {
    case STATE_METHOD: {
      // This happens when the url contains no : at all - we interpret
      // it as a file URL with no netloc
      if(url[i]=='#' || url[i]=='/') {
        i--;
        state = STATE_QUERY;
      } else if(url[i]==0) {
	buff[j]=0; j=0;
	self->query = talloc_strdup(self->ctx,buff);
	state = STATE_QUERY;
      }else if(url[i] == ':') {
	buff[j]=0; j=0;
	state = STATE_COLLON;
	self->scheme = talloc_strdup(self->ctx, buff);
      } else buff[j++] = url[i];
      break;
    };

    case STATE_COLLON: {
      if(url[i]=='/' && url[i+1]=='/') {
	state = STATE_NETLOCK;
	i+=1;
      } else state = STATE_QUERY;
      break;
    };

    case STATE_NETLOCK: {
      if(url[i]==0 || ((url[i]=='/' || url[i]=='#') && i--)) {
	state = STATE_QUERY;
	buff[j]=0; j=0;
	self->netloc = talloc_strdup(self->ctx,buff);
      } else buff[j++] = url[i];
      break;
    };

    case STATE_QUERY: {
      if(url[i]==0 || url[i]=='#') {
	state = STATE_FRAGMENT;
	buff[j]=0; j=0;
	self->query = talloc_strdup(self->ctx,buff);
      } else buff[j++] = url[i];
      break;
    };

    case STATE_FRAGMENT: {
      if(url[i]==0) {
	state = STATE_END;
	buff[j]=0; j=0;
	self->fragment = talloc_strdup(self->ctx,buff);
      } else buff[j++] = url[i];
      break;
    };

    default:
    case STATE_END:
      goto exit;

    };

    i++;
  };

exit:
  return 0;
};

struct path_element {
  char *element;
  int length;
  struct list_head list;
};

static char *collapse_path(char *query, void *ctx) {
  struct path_element *path = talloc(ctx, struct path_element);
  char *i, *j;
  struct path_element *current;
  char *result;

  INIT_LIST_HEAD(&path->list);

  // Split the path into components
  for(i=query, j=query; ; i++) {
    if(*i=='/' || *i==0) {
      current = talloc(path, struct path_element);
      current->element = talloc_size(current, i-j);
      memcpy(current->element, j, i-j);
      current->length = i-j;

      // If this element is .. we pop the last element from the list
      if(current->length == 2 && !memcmp(current->element, "..", 2)) {
        talloc_free(current);
        if(!list_empty(&path->list)) {
          list_prev(current, &path->list, list);
          list_del(&current->list);
          talloc_free(current);
        };
		// Drop /./ sequences
	  } else if(current->length==1 && current->element[0]=='.') {
		talloc_free(current);
        // Drop empty path elements
      } else if(current->length==0) {
        talloc_free(current);
      } else {
        list_add_tail(&current->list, &path->list);
      };
      j=i+1;
    };

    // The last element is null terminated
    if(*i==0) break;
  };

  result = talloc_size(ctx, i - query + 20);
  j=result;
  list_for_each_entry(current, &path->list, list) {
    // Dont write empty elements
    if(current->length == 0) continue;

    *j++ = '/';
    memcpy(j, current->element, current->length);
    j+=current->length;
  };

  *j=0;

  talloc_free(path);

  return result;
};


static char *URLParse_string(URLParse self, void *ctx) {
  char *fmt;
  char *scheme = self->scheme;
  char *query, *result;

  if(strlen(self->fragment)>0)
    fmt = "%s://%s%s#%s";
  else if(strlen(self->query)>0) {
    fmt = "%s://%s%s";
  } else
    fmt = "%s://%s";

  if(strlen(scheme)==0)
    scheme = "file";

  query = collapse_path(self->query, NULL);

  result = talloc_asprintf(ctx, fmt, scheme,
                           self->netloc,
                           query,
                           self->fragment);

  talloc_free(query);

  return result;
};

VIRTUAL(URLParse, Object) {
    VMETHOD(Con) = URLParse_Con;
    VMETHOD(string) = URLParse_string;
    VMETHOD(parse) = URLParse_parse;
} END_VIRTUAL

/** Following is the implementation of the basic RDFValue types */
/** base class */
static RDFValue RDFValue_Con(RDFValue self) {
  return self;
};

/** By default we encode into the tdb the same way as we serialise to
    RDF */
static TDB_DATA *RDFValue_encode(RDFValue self, RDFURN subject) {
  TDB_DATA *result = talloc(self, TDB_DATA);

  result->dptr = (unsigned char *)CALL(self, serialise, subject);
  if(!result->dptr) goto error;

  result->dsize = strlen((char *)result->dptr)+1;

  return result;

 error:
  talloc_free(result);
  return NULL;
};

static int RDFValue_parse(RDFValue self, char *serialised, RDFURN urn) {
  return CALL(self, decode, ZSTRING(serialised), urn);
};


VIRTUAL(RDFValue, Object) {
    VMETHOD(Con) = RDFValue_Con;
    VMETHOD(encode) = RDFValue_encode;
    VMETHOD(parse) = RDFValue_parse;

    UNIMPLEMENTED(RDFValue, clone);
    UNIMPLEMENTED(RDFValue, decode);
    UNIMPLEMENTED(RDFValue, serialise);
} END_VIRTUAL

// Encode integers for storage
static TDB_DATA *XSDInteger_encode(RDFValue self, RDFURN subject) {
  XSDInteger this = (XSDInteger)self;
  TDB_DATA *result = talloc(self, TDB_DATA);

  result->dptr = (unsigned char *)&this->value;
  result->dsize = sizeof(this->value);

  return result;
};

static XSDInteger XSDInteger_Con(XSDInteger self, uint64_t value) {
  RDFValue result = (RDFValue)self;

  CALL(result, Con);
  self->value = value;

  return self;
};

static void XSDInteger_set(XSDInteger self, uint64_t value) {
  self->value = value;

  return;
};

static int XSDInteger_decode(RDFValue this, char *data, int length, RDFURN subject) {
  XSDInteger self = (XSDInteger)this;

  self->value = 0;
  memcpy(&self->value, data, sizeof(self->value));
  return length;
};

static char *XSDInteger_serialise(RDFValue self, RDFURN subject) {
  XSDInteger this = (XSDInteger)self;

  if(this->serialised)
    talloc_unlink(self, this->serialised);

  this->serialised = talloc_asprintf(self, "%llu", (long long unsigned int)this->value);

  talloc_reference(NULL, this->serialised);

  return this->serialised;
};

static int XSDInteger_parse(RDFValue self, char *serialised, RDFURN urn) {
  XSDInteger this = (XSDInteger)self;

  this->value = strtoll(serialised, NULL, 0);

  return 1;
};

VIRTUAL(XSDInteger, RDFValue) {
   VATTR(super.raptor_type) = RAPTOR_IDENTIFIER_TYPE_LITERAL;

   // This is our official data type - or at least what raptor gives us
   VATTR(super.dataType) = DATATYPE_XSD_INTEGER;

   VMETHOD(super.encode) = XSDInteger_encode;
   VMETHOD(super.decode) = XSDInteger_decode;
   VMETHOD(super.serialise) = XSDInteger_serialise;
   VMETHOD(super.parse) = XSDInteger_parse;

   VMETHOD(Con) = XSDInteger_Con;
   VMETHOD(set) = XSDInteger_set;
} END_VIRTUAL

// Encode strings for storage
static XSDString XSDString_Con(XSDString self, char *string, int length) {
  RDFValue result = (RDFValue)self;

  // Call the super class's constructor.
  CALL(result, Con);

  if(length != 0) {
    CALL(self, set, string, length);
  };

  return self;
};

static TDB_DATA *XSDString_encode(RDFValue self, RDFURN subject) {
  XSDString this = (XSDString)self;
  TDB_DATA *result = talloc(self, TDB_DATA);

  result->dptr = (unsigned char *)this->value;
  result->dsize = this->length;

  return result;
};

static void XSDString_set(XSDString self, char *string, int length) {
  if(length < 0) {
    RaiseError(ERuntimeError, "Length is negative");
    return;
  };

  if(self->value) {
    talloc_free(self->value);
  };

  self->length = length;
  self->value = talloc_memdup(self, string, length+1);
  self->value[length]=0;
};

static int XSDString_decode(RDFValue this, char *data, int length, RDFURN subject) {
  XSDString self = (XSDString)this;

  if(self->value)
	talloc_free(self->value);

  self->value = talloc_size(self, length + 1);
  self->length = length;

  memcpy(self->value, data, length);
  self->value[length]=0;

  return length;

};

/* We serialise the string */
static char *XSDString_serialise(RDFValue self, RDFURN subject) {
  XSDString this = (XSDString)self;

  talloc_reference(NULL, this->value);

  return this->value;
};

static int XSDString_parse(RDFValue self, char *serialise, RDFURN subject) {
  XSDString this = (XSDString)self;

  this->value = talloc_strdup(self, serialise);
  this->length = strlen(this->value);

  return 1;
};

static RDFValue XSDString_clone(RDFValue self, void *ctx) {
  XSDString this = (XSDString)self;
  // Make a copy of this object
  XSDString result = talloc_memdup(ctx, self, SIZEOF(self));

  // Now make a copy of the value strings
  result->value = talloc_memdup(result, this->value, this->length);

  return (RDFValue)result;
};

VIRTUAL(XSDString, RDFValue) {
   VATTR(super.raptor_type) = RAPTOR_IDENTIFIER_TYPE_LITERAL;
   //VATTR(super.dataType) = XSD_NAMESPACE "string";
   // Strings by default dont need a dataType at all
   VATTR(super.dataType) = "";

   // Our serialised form is the same as encoded form
   VMETHOD_BASE(RDFValue, flags) = RESOLVER_ENTRY_ENCODED_SAME_AS_SERIALIZED;

   VMETHOD_BASE(RDFValue, encode) = XSDString_encode;
   VMETHOD_BASE(RDFValue, decode) = XSDString_decode;
   VMETHOD_BASE(RDFValue, serialise) = XSDString_serialise;
   VMETHOD_BASE(RDFValue, parse) = XSDString_parse;
   VMETHOD_BASE(RDFValue, clone) = XSDString_clone;

   VMETHOD(set) = XSDString_set;
   VMETHOD_BASE(XSDString, Con) = XSDString_Con;
} END_VIRTUAL 

// Encode urn for storage - we just encode the URN id of this urn and
// resolve it back through the resolver.
static TDB_DATA *RDFURN_encode(RDFValue self, RDFURN subject) {
  RDFURN this = (RDFURN)self;
  TDB_DATA *result = talloc(self, TDB_DATA);
  uint32_t *id = talloc(result, uint32_t);

  *id = CALL(oracle, get_id_by_urn, this, 1);

  result->dptr = (unsigned char *)id;
  result->dsize = sizeof(*id);

  return result;
};

static RDFValue RDFURN_Con(RDFValue self) {
  RDFURN this = (RDFURN)self;
  this->parser = CONSTRUCT(URLParse, URLParse, Con, this, NULL);
  this->value = talloc_strdup(self,"(unset)");

  return self;
};

static RDFURN RDFURN_Con2(RDFURN self, char *urn) {
  RDFValue this = (RDFValue)self;

  CALL(this, Con);
  if(urn) {
    CALL(self, set, urn);
  };

  return self;
};

static void RDFURN_set(RDFURN self, char *string) {
  CALL(self->parser, parse, string);

  // Our value is the serialsed version of what the parser got:
  if(self->value) talloc_free(self->value);
  self->value = CALL(self->parser, string, self);

  return;
};

static RDFValue RDFURN_clone(RDFValue self, void *ctx) {
  RDFURN result = talloc_memdup(ctx, self, SIZEOF(self));
  RDFURN this = (RDFURN)self;

  result->value = talloc_strdup(result, this->value);

  return (RDFValue)result;
};

static int RDFURN_decode(RDFValue this, char *data, int length, RDFURN subject) {
  RDFURN self = (RDFURN)this;
  uint32_t *id = (uint32_t *)data;

  if(length != sizeof(uint32_t))
    goto error;

  // Ask the resolver to set ourselves based on the provided id
  if(!CALL(oracle, get_urn_by_id, *id, self))
    goto error;

  return length;

 error:
  return 0;
};

/* We serialise the string */
static char *RDFURN_serialise(RDFValue self, RDFURN subject) {
  RDFURN this = (RDFURN)self;

  // This is incompatible with talloc - it has a special freer
  return (char *)raptor_new_uri((const unsigned char*)this->value);
};

static RDFURN RDFURN_copy(RDFURN self, void *ctx) {
  RDFURN result = CONSTRUCT(RDFURN, RDFValue, Con, ctx);

  CALL(result, set, self->value);

  return result;
};

static int RDFURN_parse(RDFValue self, char *serialised, RDFURN subject) {
  RDFURN_set((RDFURN)self, serialised);
  return 1;
};

// parse filename as a URL and if its relative, append it to
// ourselves. If filename is an absolute URL we replace ourselve with
// it.
static void RDFURN_add(RDFURN self,  char *filename) {
  URLParse parser = CONSTRUCT(URLParse, URLParse, Con, self, filename);

  // Absolute URL
  if(strlen(parser->scheme)>0) {
    talloc_free(self->parser);
    talloc_free(self->value);

    self->parser = parser;
    self->value = CALL(parser, string, self);
  } else {
    char *seperator = "/";

    if(strlen(self->parser->query)==0) seperator="";

    // Relative URL
    self->parser->query = talloc_asprintf(self, "%s%s%s",
					  self->parser->query,
					  seperator,
					  parser->query);

    self->parser->fragment = talloc_asprintf(self, "%s%s",
                                             self->parser->fragment,
                                             parser->fragment);
    talloc_free(self->value);
    self->value = CALL(self->parser, string, self);
    talloc_free(parser);
  };
};

/** This adds the binary buffer to the URL by escaping it
    appropriately */
static void RDFURN_add_query(RDFURN self, unsigned char *query, unsigned int len) {
  char *buffer = escape_filename(self, (const char *)query, len);

  RDFURN_add(self, buffer);

  talloc_free(buffer);
};

/** Compares our current URN to the volume. If we share a common base,
    return a relative URN */
TDB_DATA RDFURN_relative_name(RDFURN self, RDFURN volume) {
  TDB_DATA result;

  if(startswith(self->value, volume->value)) {
    result.dptr = (unsigned char *)self->value + strlen(volume->value) + 1;
    result.dsize = strlen((char *)result.dptr) +1;
  } else {
    result = tdb_data_from_string(self->value);
  };

  return result;
};

VIRTUAL(RDFURN, RDFValue) {
   VATTR(super.raptor_type) = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
   VATTR(super.dataType) = DATATYPE_RDF_URN;

   VMETHOD_BASE(RDFValue, Con) = RDFURN_Con;
   VMETHOD_BASE(RDFValue, encode) = RDFURN_encode;
   VMETHOD_BASE(RDFValue, decode) = RDFURN_decode;
   VMETHOD_BASE(RDFValue, serialise) = RDFURN_serialise;
   VMETHOD_BASE(RDFValue, parse) = RDFURN_parse;
   VMETHOD_BASE(RDFValue, clone) = RDFURN_clone;


   VMETHOD(Con) = RDFURN_Con2;
   VMETHOD(set) = RDFURN_set;
   VMETHOD(add) = RDFURN_add;
   VMETHOD(add_query) = RDFURN_add_query;
   VMETHOD(copy) = RDFURN_copy;
   VMETHOD(relative_name) = RDFURN_relative_name;
} END_VIRTUAL

static TDB_DATA *XSDDatetime_encode(RDFValue self, RDFURN subject) {
  XSDDatetime this = (XSDDatetime)self;
  TDB_DATA *result = talloc(self, TDB_DATA);

  result->dptr = (unsigned char *)&this->value;
  result->dsize = sizeof(this->value);

  return result;
};

static int XSDDatetime_decode(RDFValue self, char *data, int length, RDFURN subject) {
  XSDDatetime this = (XSDDatetime)self;

  memcpy(&this->value, data, sizeof(this->value));
  return length;
};

static void XSDDatetime_set(XSDDatetime self, struct timeval time) {
  self->value = time;

  return;
};

static RDFValue XSDDatetime_clone(RDFValue self, void *ctx) {
  XSDDatetime result = talloc_memdup(ctx, self, SIZEOF(self));
  XSDDatetime this = (XSDDatetime)self;

  result->value = this->value;

  return (RDFValue)result;
};

#define DATETIME_FORMAT_STR "%Y-%m-%dT%H:%M:%S"

static char *XSDDatetime_serialise(RDFValue self, RDFURN subject) {
  XSDDatetime this = (XSDDatetime)self;
  char buff[BUFF_SIZE];

  if(this->serialised) talloc_unlink(self, this->serialised);

  if(BUFF_SIZE > strftime(buff, BUFF_SIZE, DATETIME_FORMAT_STR,
			  localtime(&this->value.tv_sec))) {
    this->serialised = talloc_asprintf(this, "%s.%06u+%02u:%02u", buff,
                                       0,
				       //(unsigned int)this->value.tv_usec,
				       (unsigned int)this->gm_offset / 3600,
				       (unsigned int)this->gm_offset % 3600);

    talloc_reference(NULL, this->serialised);
    return this->serialised;
  };

  return NULL;
};

static int XSDDatetime_parse(RDFValue self, char *serialised, RDFURN subject) {
  XSDDatetime this = (XSDDatetime)self;
  struct tm time;

  memset(&time, 0, sizeof(time));
  strptime(serialised, DATETIME_FORMAT_STR, &time);

  time.tm_isdst = -1;
  this->value.tv_sec = mktime(&time);

  return 1;
};

static RDFValue XSDDatetime_Con(RDFValue self) {
  XSDDatetime this = (XSDDatetime)self;
  gettimeofday(&this->value, NULL);

  return self;
};

VIRTUAL(XSDDatetime, RDFValue) {
  VMETHOD_BASE(RDFValue, raptor_type) = RAPTOR_IDENTIFIER_TYPE_LITERAL;
  VMETHOD_BASE(RDFValue, dataType) = DATATYPE_XSD_DATETIME;

  VMETHOD_BASE(RDFValue, encode) = XSDDatetime_encode;
  VMETHOD_BASE(RDFValue, decode) = XSDDatetime_decode;
  VMETHOD_BASE(RDFValue, serialise) = XSDDatetime_serialise;
  VMETHOD_BASE(RDFValue, parse) = XSDDatetime_parse;
  VMETHOD_BASE(RDFValue, Con) = XSDDatetime_Con;
  VMETHOD_BASE(RDFValue, clone) = XSDDatetime_clone;

   VMETHOD(set) = XSDDatetime_set;
} END_VIRTUAL


/** This is the Bevy Index RDF type. */
static RDFValue IntegerArrayBinary_Con(RDFValue this) {
  IntegerArrayBinary self = (IntegerArrayBinary)this;

  self->alloc_size = 0;
  self->array = NULL;
  self->current = 0;
  self->extension = talloc_strdup(self, self->extension);

  return SUPER(RDFValue, RDFValue, Con);
};

static int IntegerArrayBinary_decode(RDFValue self, char *data, int length, RDFURN subject) {
  /* We interpret the data as a suffix to our present URL or a fully
     qualified URL. */
  FileLikeObject fd;
  IntegerArrayBinary this = (IntegerArrayBinary)self;
  RDFURN array_urn = CALL(subject, copy, self);
  int i;
  uint32_t *ptr;

  // We use the data given as an extension
  if(this->extension) talloc_free(this->extension);
  this->extension = talloc_strdup(this, data);

  CALL(array_urn, add, this->extension);

  // Now we read the index segment
  fd = (FileLikeObject)CALL(oracle, open, array_urn, 'r');
  if(!fd) {
    goto error;
  };

  /** This holds the index into the segment of all the chunks.
      It is an array of chunks_in_segment ints long. NOTE - on disk
      all uint32_t are encoded in big endian format.
  */
  ptr = (uint32_t *)CALL(fd, get_data);

  this->array = talloc_size(self, fd->size->value);
  this->size = fd->size->value / sizeof(*this->array);

  // Fix up endianess:
  for(i=0;i<this->size; i++) {
    this->array[i] = ntohl(ptr[i]);
    this->current ++;
  };

  CALL((AFFObject)fd, cache_return);

  return 1;

 error:
  return 0;
};

static char *IntegerArrayBinary_serialise(RDFValue self, RDFURN subject) {
  IntegerArrayBinary this = (IntegerArrayBinary) self;
  RDFURN segment = CALL(subject, copy, self);
  RDFURN stored = new_RDFURN(segment);
  XSDInteger size = new_XSDInteger(segment);
  AFF4Volume volume;

  CALL(segment, add, this->extension);

  // Check if the segment is already written and its the same size we
  // dont do anything with it
  if(CALL(oracle, resolve_value, segment, AFF4_VOLATILE_SIZE, (RDFValue)size) &&\
     size->value == this->size * sizeof(*this->array))
    goto exit;

  if(!CALL(oracle, resolve_value, subject, AFF4_VOLATILE_STORED, (RDFValue)stored) && \
     !CALL(oracle, resolve_value, subject, AFF4_STORED, (RDFValue)stored)) {
    RaiseError(ERuntimeError, "Subject URN %s has no storage", subject->value);
    goto error;
  };

  volume = (AFF4Volume)CALL(oracle, open, stored, 'w');
  if(!volume) {
    RaiseError(ERuntimeError, "Unable to open storage for %s", subject->value);
    goto error;
  } else {
    uint32_t *stored_array = talloc_array_size(segment, sizeof(*this->array), this->size);
    int i;
    for(i=0; i<this->size; i++) {
      stored_array[i] = htonl(this->array[i]);
    };

    if(-1==CALL(volume, writestr, segment->value,
                (char *)stored_array, this->size * sizeof(uint32_t), ZIP_STORED)) {
      PUSH_ERROR_STATE;
      CALL((AFFObject)volume, cache_return);
      POP_ERROR_STATE;

      RaiseError(ERuntimeError, "Unable to write binary segment");

      goto error;
    };

    CALL((AFFObject)volume, cache_return);
  };

 exit:
  talloc_free(segment);
  return talloc_strdup(self,this->extension);

 error:
  talloc_free(segment);
  return NULL;
};

static void IntegerArrayBinary_add(IntegerArrayBinary self, unsigned int item) {
  while(!self->array || self->current >= self->alloc_size) {
    self->alloc_size += BUFF_SIZE;
    self->array = talloc_realloc_size(self, self->array,
                                      sizeof(uint32_t) * self->alloc_size);
  };

  self->array[self->current++] = item;
  self->size = max(self->current, self->size);
};

static RDFValue IntegerArrayBinary_clone(RDFValue self, void *ctx) {
  IntegerArrayBinary result = talloc_memdup(ctx, self, SIZEOF(self));
  IntegerArrayBinary this = (IntegerArrayBinary)self;

  result->array = talloc_memdup(result, this->array, this->size);

  return (RDFValue)result;
};

VIRTUAL(IntegerArrayBinary, RDFValue) {
  VATTR(super.raptor_type) = RAPTOR_IDENTIFIER_TYPE_LITERAL;
  VATTR(super.dataType) = AFF4_INTEGER_ARRAY_BINARY;

  VATTR(extension) = "index";

  // Our serialised form is the same as encoded form
  VMETHOD_BASE(RDFValue, flags) = RESOLVER_ENTRY_ENCODED_SAME_AS_SERIALIZED;

  VMETHOD_BASE(RDFValue, Con) = IntegerArrayBinary_Con;
  VMETHOD_BASE(RDFValue, decode) = IntegerArrayBinary_decode;
  VMETHOD_BASE(RDFValue, serialise) = IntegerArrayBinary_serialise;
  VMETHOD_BASE(RDFValue, clone) = IntegerArrayBinary_clone;

  VMETHOD_BASE(IntegerArrayBinary, add) = IntegerArrayBinary_add;
} END_VIRTUAL

static char *IntegerArrayInline_serialise(RDFValue self, RDFURN subject) {
  IntegerArrayBinary this = (IntegerArrayBinary)self;
  int available = this->size * 10;
  char *buffer = talloc_zero_size(self, available + 1);
  int i,buffer_ptr=0;

  for(i=0; i < this->size && buffer_ptr < available; i++) {
    buffer_ptr += snprintf(buffer + buffer_ptr, available - buffer_ptr, "%u,", this->array[i]);
  };

  return buffer;
};

static int IntegerArrayInline_decode(RDFValue self, char *data, int length, RDFURN subject) {
  uint32_t number;
  char *x, *y=data;

  for(x=data; x-data < length && *x; x++) {
    if(*x==',') {
      *x=0;
      number = atol(y);
      CALL((IntegerArrayBinary)self, add, number);
      y = x+1;
    };
  };

  return 1;
};

VIRTUAL(IntegerArrayInline, IntegerArrayBinary) {
  VMETHOD_BASE(RDFValue, dataType) = AFF4_INTEGER_ARRAY_INLINE;

  // Our serialised form is the same as encoded form
  VMETHOD_BASE(RDFValue, flags) = RESOLVER_ENTRY_ENCODED_SAME_AS_SERIALIZED;

  VMETHOD_BASE(RDFValue, decode) = IntegerArrayInline_decode;
  VMETHOD_BASE(RDFValue, serialise) = IntegerArrayInline_serialise;
} END_VIRTUAL


/* This RDF_Registry is a lookup table of class handlers and their
   IDs.

   The way this works is that when we initialise all modules register
   their class handlers in this Registry array. If we encounter these
   dataTypes when parsing an RDF file, we instantiate an object from
   the class registery and call its parse method to deserialise it.

   You can extend this by creating your own private dataTypes - you
   will need to subclass RDFValue and call register_rdf_value_class()
   to ensure your class is registered - this way when we encounter
   such an item in the information file we will call your class to
   parse it.
 */
Cache RDF_Registry = NULL;

void register_rdf_value_class(RDFValue classref) {
  if(!RDF_Registry) {
    RDF_Registry = CONSTRUCT(Cache, Cache, Con, NULL, 100, 0);
    talloc_set_name_const(RDF_Registry, "RDFValue dispatcher");
  };

  if(!classref->dataType) {
    AFF4_ABORT("No dataType specified in %s", NAMEOF(classref));
  };

  if(!CALL(RDF_Registry, present, ZSTRING(classref->dataType))) {
    Object tmp = talloc_memdup(NULL, classref, SIZEOF(classref));

    talloc_set_name(tmp, "RDFValue type %s", NAMEOF(classref));
    CALL(RDF_Registry, put, ZSTRING(classref->dataType), tmp);

    talloc_unlink(NULL, tmp);
  };
};

/** This function initialises the RDF types registry. */
AFF4_MODULE_INIT(A000_rdf) {
  raptor_init();

  // Register all the known basic types
  register_rdf_value_class((RDFValue)GETCLASS(XSDInteger));
  register_rdf_value_class((RDFValue)GETCLASS(XSDString));
  register_rdf_value_class((RDFValue)GETCLASS(RDFURN));
  register_rdf_value_class((RDFValue)GETCLASS(XSDDatetime));
  register_rdf_value_class((RDFValue)GETCLASS(IntegerArrayBinary));
  register_rdf_value_class((RDFValue)GETCLASS(IntegerArrayInline));
};

/** RDF parsing */
static void triples_handler(void *data, const raptor_statement* triple) 
{
  RDFParser self = (RDFParser)data;
  char *urn_str, *attribute, *value_str, *type_str;
  RDFValue class_ref = (RDFValue)GETCLASS(XSDString);
  RDFValue result = NULL;

  // Ignore anonymous and invalid triples.
  if(triple->subject_type != RAPTOR_IDENTIFIER_TYPE_RESOURCE ||
     triple->predicate_type != RAPTOR_IDENTIFIER_TYPE_RESOURCE)
    return;

  /* do something with the triple */
  urn_str = (char *)raptor_uri_as_string((raptor_uri *)triple->subject);
  attribute = (char *)raptor_uri_as_string((raptor_uri *)triple->predicate);
  value_str = (char *)raptor_uri_as_string((raptor_uri *)triple->object);
  type_str = (char *)raptor_uri_as_string((raptor_uri *)triple->object_literal_datatype);

  CALL(self->urn, set, urn_str);

  if(strcmp(self->volume_urn->value, urn_str)) {
    if(!CALL(self->member_cache, present, ZSTRING(urn_str))) {
      // Make sure the volume contains this object
      printf("!!! %s contains %s\n", self->volume_urn->value, urn_str);

      CALL(oracle, add_value, self->volume_urn, AFF4_VOLATILE_CONTAINS,
           (RDFValue)self->urn,0);

      CALL(self->member_cache, put, ZSTRING(urn_str), NULL);
    };
  };

  if(triple->object_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE) {
    class_ref = (RDFValue)GETCLASS(RDFURN);
  } else if(triple->object_literal_datatype) {
    class_ref = (RDFValue)CALL(RDF_Registry, borrow,
                               ZSTRING(type_str));
  };

  result = CONSTRUCT_FROM_REFERENCE(class_ref, Con, NULL);
  if(result) {
    CALL(result, parse, value_str, self->urn);
    CALL(oracle, add_value, self->urn, attribute, (RDFValue)result,0);
    talloc_free(result);
  };
}

static void message_handler(void *data, raptor_locator* locator, 
			    const char *message)
{
  printf("%s\n", message);
}

static void fatal_error_handler(void *data, raptor_locator *locator,
			  const char *message) {
  RDFParser self = (RDFParser)data;
  // An error occured - copy the message and to the caller.
  strncpy(self->message, message, BUFF_SIZE);
  self->message[BUFF_SIZE]=0;
  longjmp(self->env, 1);
};


static int RDFParser_parse(RDFParser self, FileLikeObject fd, char *format, char *base) {
  raptor_parser* rdf_parser;
  raptor_uri uri=NULL;

  // Take a sensible default
  if(!format) format = "turtle";

  rdf_parser = raptor_new_parser(format);
  if(!rdf_parser) {
    RaiseError(ERuntimeError, "Unable to create parser for RDF serialization %s", format);
    goto error;
  };

  CALL(self->volume_urn ,set , base);

  // Dont talk to the internet
  raptor_set_feature(rdf_parser, RAPTOR_FEATURE_NO_NET, 1);
  raptor_set_statement_handler(rdf_parser, self, self->triples_handler);

  raptor_set_fatal_error_handler(rdf_parser, self, fatal_error_handler);
  raptor_set_error_handler(rdf_parser, self, self->message_handler);
  raptor_set_warning_handler(rdf_parser, self, self->message_handler);

  if(base)
    uri = raptor_new_uri((const unsigned char*)base);

  /* FIXME - handle errors using a message handler. We need to setjmp
     it here because the raptor library will abort() if the error
     handler returns. We need to unwind the stack and raise an error
     instead.
  */
  if(setjmp(self->env)!=0) {
    // Oops an error occured
    goto error;
  };

  raptor_start_parse(rdf_parser, uri);
  while(1) {
    // Read some data from our fd
    unsigned char buff[BUFF_SIZE];
    int len = CALL(fd, read, (char *)buff, BUFF_SIZE);

    if(len<=0) break;
    // Shove it into the parser.
    raptor_parse_chunk(rdf_parser, buff, len, 0);
  }
  // Done - flush the parser
  raptor_parse_chunk(rdf_parser, NULL, 0, 1); /* no data and is_end =
						 1 */
  // Cleanup
  if(uri)
    raptor_free_uri((raptor_uri *)uri);
  raptor_free_parser(rdf_parser);
  return 1;

 error:
  if(uri)
    raptor_free_uri((raptor_uri *)uri);
  raptor_free_parser(rdf_parser);

  return 0;
};

static RDFParser RDFParser_Con(RDFParser self) {
  self->urn = new_RDFURN(self);
  self->volume_urn = new_RDFURN(self);
  self->member_cache = CONSTRUCT(Cache, Cache, Con, self, 100, 0);
  return self;
};

VIRTUAL(RDFParser, Object) {
     VMETHOD(triples_handler) = triples_handler;
     VMETHOD(message_handler) = message_handler;
     VMETHOD(parse) = RDFParser_parse;
     VMETHOD(Con) = RDFParser_Con;
} END_VIRTUAL

/*** RDF serialization methods */

static int iostream_write_byte(void *context, const int byte) {
  RDFSerializer self=(RDFSerializer)context;
  self->buff[self->i] = (char)byte;
  self->i++;

  if(self->i >= BUFF_SIZE) {
    CALL(self->fd, write, self->buff, self->i);
    self->i = 0;
  };

  return 1;
};

static int iostream_write_bytes(void *context, const void *ptr, size_t size, size_t nmemb) {
  RDFSerializer self=(RDFSerializer)context;
  unsigned int length = nmemb * size;
  char *src = (char *)ptr;
  unsigned int available;

  if(self->i > BUFF_SIZE) abort();

  while(length > 0) {
    available = min(length, BUFF_SIZE - self->i);
    if(available <= 0) {
      CALL(self->fd, write, self->buff, self->i);
      self->i = 0;
      continue;
    };

    memcpy(self->buff + self->i, src, available);
    self->i += available;
    src += available;
    length -= available;
  };

  return size * nmemb;
}

raptor_iostream_handler2 raptor_special_handler = {
  .version = 2,
  .write_byte = iostream_write_byte,
  .write_bytes = iostream_write_bytes,
};

static int tdb_attribute_extract(TDB_CONTEXT *tdb, TDB_DATA key, 
				       TDB_DATA value, void *data) {
  RDFSerializer self=(RDFSerializer) data;

  // Only look at proper attributes
  if(memcmp(key.dptr,"http",4))
    goto exit;

  // Skip serializing of volatile triples
  if(!memcmp(key.dptr, ZSTRING_NO_NULL(VOLATILE_NS)))
    goto exit;

  if(!memcmp(key.dptr, ZSTRING_NO_NULL(XSD_NAMESPACE)))
    goto exit;

  {
    RDFURN s = new_RDFURN(self->attributes);

    CALL(s, set, (char *)key.dptr);
    CALL(self->attributes, put, (char *)key.dptr, key.dsize, (Object)s);
  };

 exit:
  return 0;
};

static int RDFSerializer_destructor(void *this) {
  RDFSerializer self = (RDFSerializer)this;

  raptor_free_iostream(self->iostream);

  return 0;
};

static RDFSerializer RDFSerializer_Con(RDFSerializer self, char *base, 
                                       FileLikeObject fd) {
  char *type = "turtle";

  // We keep a reference to the FileLikeObject (although we dont
  // technically own it) to ensure that it doesnt get freed from under
  // us.
  if(!fd) goto error;

  self->fd = fd;
  (void)talloc_reference(self, fd);

  // We need to cache the attributes because traversing the TDB is too
  // slow all the time.
  self->attributes = CONSTRUCT(Cache, Cache, Con, self, 100, 0);

  // Get a copy of all the attributes
  tdb_traverse_read(oracle->attribute_db, tdb_attribute_extract, self);

  self->iostream = raptor_new_iostream_from_handler2((void *)self, &raptor_special_handler);
  // Try to make a new serialiser
  self->rdf_serializer = raptor_new_serializer(type);
  if(!self->rdf_serializer) {
    RaiseError(ERuntimeError, "Cant create serializer of type %s", type);
    goto error;
  };

  if(base) {
    raptor_uri uri = raptor_new_uri((const unsigned char*)base);
    raptor_serialize_start(self->rdf_serializer,
			   uri, self->iostream);

    raptor_free_uri(uri);
    uri = (void*)raptor_new_uri((const unsigned char*)PREDICATE_NAMESPACE);
    raptor_serialize_set_namespace(self->rdf_serializer, uri, (unsigned char *)"aff4");
    raptor_free_uri(uri);

    uri = (void*)raptor_new_uri((const unsigned char*)XSD_NAMESPACE);
    raptor_serialize_set_namespace(self->rdf_serializer, uri, (unsigned char *)"xsd");
    raptor_free_uri(uri);
  };


  talloc_set_destructor((void *)self, RDFSerializer_destructor);

  return self;

 error:
  talloc_free(self);
  return NULL;
};

static void RDFSerializer_set_namespace(RDFSerializer self, char *prefix, char *namespace) {
  raptor_uri uri = (void*)raptor_new_uri((const unsigned char*)prefix);
  raptor_serialize_set_namespace(self->rdf_serializer, uri, (unsigned char *)namespace);
  raptor_free_uri(uri);
};

static int RDFSerializer_serialize_statement(RDFSerializer self,
                                             RESOLVER_ITER *iter,
                                             RDFURN urn,
                                             RDFURN attribute) {
  raptor_statement triple;
  void *raptor_urn;
  RDFValue value = NULL;

  raptor_urn = (void*)raptor_new_uri((const unsigned char*)urn->value);

  memset(&triple, 0, sizeof(triple));

  // If the encoding is the same as serialised version we can
  // just copy it into the serialiser directly from the iterator.
  if(iter->head.flags & RESOLVER_ENTRY_ENCODED_SAME_AS_SERIALIZED) {
    triple.object = CALL(oracle, encoded_data_from_iter, &value, iter);
    if(!triple.object)
      goto error;
  } else {
    value = CALL(oracle, alloc_from_iter, raptor_urn, iter);

    if(!value) goto error;

    // New reference here:
    triple.object = CALL(value, serialise, urn);
  };

  if(!triple.object) {
    AFF4_LOG(AFF4_LOG_MESSAGE, AFF4_SERVICE_RDF_SUBSYSYEM,
             urn,
             "Unable to serialise attribute %s\n",
             attribute->value);
    goto error;
  };

  triple.subject = raptor_urn;
  triple.subject_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

  triple.predicate = (void*)raptor_new_uri((const unsigned char*)attribute->value);
  triple.predicate_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
  triple.object_type = value->raptor_type;

  // Default to something sensible
  if(RAPTOR_IDENTIFIER_TYPE_UNKNOWN == triple.object_type)
    triple.object_type = RAPTOR_IDENTIFIER_TYPE_LITERAL;

  // If the dataType is emptry just have a NULL
  // object_literal_datatype:
  triple.object_literal_datatype = value->dataType[0] ?                             \
    raptor_new_uri((const unsigned char*)value->dataType) : 0;

  raptor_serialize_statement(self->rdf_serializer, &triple);
  raptor_free_uri((raptor_uri*)triple.predicate);
  if(triple.object_literal_datatype)
    raptor_free_uri((raptor_uri*)triple.object_literal_datatype);

  // Special free function for URIs
  if(triple.object_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE)
    raptor_free_uri((raptor_uri*)triple.object);
  else
    talloc_unlink(NULL, (void *)triple.object);

  talloc_free(raptor_urn);
  return 1;

 error:
  talloc_free(raptor_urn);
  return 0;
};

static int RDFSerializer_serialize_urn(RDFSerializer self, 
				       RDFURN urn) {
  Cache i;

  printf(".");
  fflush(stdout);

  // Iterate over all attributes
  list_for_each_entry(i, &self->attributes->cache_list, cache_list) {
    RESOLVER_ITER *iter;
    RDFURN attribute = (RDFURN)i->data;

    iter = CALL(oracle, get_iter, NULL, urn, attribute->value);

    // Now iterate over all the values for this
    while(iter && iter->offset > 0) {
      RDFSerializer_serialize_statement(self, iter, urn, attribute);
    };

    talloc_free(iter);
  };

  self->count ++;
  if(self->count > 100) {
    self->count =0;
    raptor_serialize_end(self->rdf_serializer);
  };

  return 1;
};

static void RDFSerializer_close(RDFSerializer self) {
  raptor_serialize_end(self->rdf_serializer);

  // Flush the buffer
  CALL(self->fd, write, self->buff, self->i);

  raptor_free_serializer(self->rdf_serializer);

  talloc_free(self);
};

VIRTUAL(RDFSerializer, Object) {
     VMETHOD(Con) = RDFSerializer_Con;
     VMETHOD(serialize_urn) = RDFSerializer_serialize_urn;
     VMETHOD(serialize_statement) = RDFSerializer_serialize_statement;
     VMETHOD(set_namespace) = RDFSerializer_set_namespace;
     VMETHOD(close) = RDFSerializer_close;
} END_VIRTUAL


/*** Convenience functions */
RDFValue rdfvalue_from_int(void *ctx, uint64_t value) {
  XSDInteger result = new_XSDInteger(ctx);

  result->set(result, value);
  return (RDFValue)result;
};

RDFValue rdfvalue_from_urn(void *ctx, char *value) {
  RDFURN result = new_RDFURN(ctx);
  if(!value) return NULL;
  result->set(result, value);

  return (RDFValue)result;
};

RDFValue rdfvalue_from_string(void *ctx, char *value) {
  XSDString result = CONSTRUCT(XSDString, RDFValue, Con, ctx);

  result->set(result, value, strlen(value));

  return (RDFValue)result;
};

DLL_PUBLIC RDFURN new_RDFURN(void *ctx) {
  return CONSTRUCT(RDFURN, RDFValue, Con, ctx);
};

DLL_PUBLIC XSDInteger new_XSDInteger(void *ctx) {
  return CONSTRUCT(XSDInteger, RDFValue, Con, ctx);
};

DLL_PUBLIC XSDString new_XSDString(void *ctx) {
  return CONSTRUCT(XSDString, RDFValue, Con, ctx);
};

DLL_PUBLIC XSDDatetime new_XSDDateTime(void *ctx) {
  return CONSTRUCT(XSDDatetime, RDFValue, Con, ctx);
};

// Uses the class registry to construct a type by name
DLL_PUBLIC RDFValue new_rdfvalue(void *ctx, char *type) {
  RDFValue result = NULL;
  RDFValue class_ref = (RDFValue)CALL(RDF_Registry, borrow,
                                      ZSTRING(type));

  if(class_ref) {
    result = CONSTRUCT_FROM_REFERENCE(class_ref, Con, ctx);
  };

  return result;
};
