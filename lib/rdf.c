/** This handles the RDF serialization, and parsing from the
    resolver. 
*/
/** The following are related to raptor - RDF serialization and
    parsing 
*/
#include "aff4.h"
#include "aff4_rdf_serialise.h"
#include "aff4_rdf.h"

/** Parsing URLs - basically a state machine */
enum url_state {
  STATE_METHOD,
  STATE_COLLON,
  STATE_NETLOCK,
  STATE_QUERY,
  STATE_FRAGMENT,
  STATE_END
};

URLParse URLParse_Con(URLParse self, char *url) {
  CALL(self, parse ,url);
  return self;
};

int URLParse_parse(URLParse self, char *url) {
  int i=0,j=0,end=0;
  char buff[BUFF_SIZE];
  enum url_state state = STATE_METHOD;

  // Its ok to call us with NULL - we just wont parse anything.
  if(!url) goto exit;

  // Initialise with defaults:
  self->scheme = "";
  self->netloc = "";
  self->query = "";	      
  self->fragment = "";

  while(!end) {
    if(url[i]==0) end=1;

    switch(state) {
    case STATE_METHOD: {
      // This happens when the url contains no : at all - we interpret
      // it as a file URL with no netloc
      if(url[i]==0 || url[i]=='/') {
	buff[j]=0; j=0;
	self->query = talloc_strdup(self,buff);
	state = STATE_QUERY;
      }else if(url[i] == ':') {
	buff[j]=0; j=0;
	state = STATE_COLLON;
	self->scheme = talloc_strdup(self, buff);
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
      if(url[i]==0 || url[i]=='/') {
	state = STATE_QUERY;
	buff[j]=0; j=0;
	self->netloc = talloc_strdup(self,buff);
      } else buff[j++] = url[i];
      break;
    };

    case STATE_QUERY: {
      if(url[i]==0 || url[i]=='#') {
	state = STATE_FRAGMENT;
	buff[j]=0; j=0;
	self->query = talloc_strdup(self,buff);
      } else buff[j++] = url[i];
      break;
    };

    case STATE_FRAGMENT: {
      if(url[i]==0) {
	state = STATE_END;
	buff[j]=0; j=0;
	self->fragment = talloc_strdup(self,buff);
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

char *URLParse_string(URLParse self, void *ctx) {
  char *fmt;
  char *scheme = self->scheme;

  if(strlen(self->fragment)>0)
    fmt = "%s://%s/%s#%s";
  else if(strlen(self->query)>0)
    fmt = "%s://%s/%s";
  else
    fmt = "%s://%s";

  if(strlen(scheme)==0)
    scheme = "file";

  return talloc_asprintf(ctx, fmt, scheme,
			 self->netloc, self->query,
			 self->fragment);
};

VIRTUAL(URLParse, Object)
    VMETHOD(Con) = URLParse_Con;
    VMETHOD(string) = URLParse_string;
    VMETHOD(parse) = URLParse_parse;
END_VIRTUAL

/** Following is the implementation of the basic RDFValue types */
/** base class */
static RDFValue RDFValue_Con(RDFValue self) {
  return self;
};

VIRTUAL(RDFValue, Object)
    VMETHOD(Con) = RDFValue_Con;
END_VIRTUAL

// Encode integers for storage
static TDB_DATA *XSDInteger_encode(RDFValue self) {
  XSDInteger this = (XSDInteger)self;
  TDB_DATA *result = talloc(self, TDB_DATA);

  result->dptr = (unsigned char *)&this->value;
  result->dsize = sizeof(this->value);

  return result;
};

static RDFValue XSDInteger_set(XSDInteger self, uint64_t value) {
  self->value = value;

  return (RDFValue)self;
};

static int XSDInteger_decode(RDFValue this, int fd, int length) {
  XSDInteger self = (XSDInteger)this;

  self->value = 0;
  return read(fd, &self->value, length);
};

static char *XSDInteger_serialise(RDFValue self) {
  XSDInteger this = (XSDInteger)self;
  
  return talloc_asprintf(self, "%llu", this->value);
};

VIRTUAL(XSDInteger, RDFValue)
   VATTR(super.dataType) = "xsd:integer";
   VMETHOD(super.encode) = XSDInteger_encode;
   VMETHOD(super.decode) = XSDInteger_decode;
   VMETHOD(super.serialise) = XSDInteger_serialise;

   VMETHOD(set) = XSDInteger_set;
END_VIRTUAL


// Encode strings for storage
static TDB_DATA *XSDString_encode(RDFValue self) {
  XSDString this = (XSDString)self;
  TDB_DATA *result = talloc(self, TDB_DATA);

  result->dptr = (unsigned char *)this->value;
  result->dsize = this->length;

  return result;
};

static RDFValue XSDString_set(XSDString self, char *string, int length) {
  if(self->value) {
    talloc_free(self->value);
  };

  self->length = length;
  self->value = talloc_memdup(self, string, length);

  return (RDFValue)self;
};

static int XSDString_decode(RDFValue this, int fd, int length) {
  XSDString self = (XSDString)this;

  self->value = talloc_realloc_size(self, self->value, length + 1);
  self->length = length;

  self->value[length]=0;

  return read(fd, self->value, length);
};

/* We serialise the string */
static char *XSDString_serialise(RDFValue self) {
  XSDString this = (XSDString)self;
  
  return talloc_memdup(self, this->value, this->length);
};

VIRTUAL(XSDString, RDFValue)
   VATTR(super.dataType) = "xsd:string";
   VMETHOD(super.encode) = XSDString_encode;
   VMETHOD(super.decode) = XSDString_decode;
   VMETHOD(super.serialise) = XSDString_serialise;

   VMETHOD(set) = XSDString_set;
END_VIRTUAL

// Encode urn for storage
static TDB_DATA *RDFURN_encode(RDFValue self) {
  RDFURN this = (RDFURN)self;
  TDB_DATA *result = talloc(self, TDB_DATA);

  result->dptr = (unsigned char *)this->value;
  result->dsize = strlen(this->value)+1;

  return result;
};

static RDFValue RDFURN_Con(RDFValue self) {
  RDFURN this = (RDFURN)self;
  this->parser = CONSTRUCT(URLParse, URLParse, Con, this, NULL);

  return self;
};

static RDFValue RDFURN_set(RDFURN self, char *string) {
  int length;

  CALL(self->parser, parse, string);

  // Our value is the serialsed version of what the parser got:
  if(self->value) talloc_free(self->value);
  self->value = CALL(self->parser, string, self);

  return self;
};

static int RDFURN_decode(RDFValue this, int fd, int length) {
  RDFURN self = (RDFURN)this;
  self->value = talloc_realloc_size(self, self->value, length);

  read(fd, self->value, length);
  CALL(self->parser, parse, self->value);

  return length;
};

/* We serialise the string */
static char *RDFURN_serialise(RDFValue self) {
  RDFURN this = (RDFURN)self;
  
  return talloc_strdup(self, this->value);
};

static RDFURN RDFURN_copy(RDFURN self, void *ctx) {
  RDFURN result = CONSTRUCT(RDFURN, RDFValue, super.Con, ctx);

  CALL(result, set, self->value);

  return result;
};

static char *illegal_url_chars = "|?[]\\+<>:;\'\",*&";
static char illegal_url_lut[128];
static void init_rdf_luts() {
  char *i;
  
  memset(illegal_url_lut, 0, 
	 sizeof(illegal_url_lut));

  for(i=illegal_url_chars;*i;i++) 
    illegal_url_lut[(int)*i]=1;
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
    talloc_free(self->value);
    self->value = CALL(self->parser, string, self);
    talloc_free(parser);
  };
};

/** Compares our current URN to the volume. If we share a common base,
    return a relative URN */
TDB_DATA RDFURN_relative_name(RDFURN self, RDFURN volume) {
  TDB_DATA result;

  if(startswith(self->value, volume->value)) {
    result.dptr = self->value + strlen(volume->value) + 1;
    result.dsize = strlen(result.dptr) + 1;
  } else {
    result = tdb_data_from_string(self->value);
  };

  return result;
};

VIRTUAL(RDFURN, RDFValue)
   VATTR(super.dataType) = "rdf:urn";
   VMETHOD(super.Con) = RDFURN_Con;
   VMETHOD(super.encode) = RDFURN_encode;
   VMETHOD(super.decode) = RDFURN_decode;
   VMETHOD(super.serialise) = RDFURN_serialise;

   VMETHOD(set) = RDFURN_set;
   VMETHOD(add) = RDFURN_add;
   VMETHOD(copy) = RDFURN_copy;
   VMETHOD(relative_name) = RDFURN_relative_name;
END_VIRTUAL


/* This RDF_Registry is a lookup table of class handlers and their
   IDs.

   The way this works is that when we initialise all modules register
   their class handlers in this Registry array

 */
Cache RDF_Registry = NULL;
		
void register_rdf_value_class(RDFValue classref) {
  Cache tmp;

  if(!RDF_Registry) {
    RDF_Registry = CONSTRUCT(Cache, Cache, Con, NULL, 100, 0);
    RDF_Registry->static_objects = 1;
  };

  tmp = CALL(RDF_Registry, get, ZSTRING_NO_NULL(classref->dataType));
  if(!tmp) {
    CALL(RDF_Registry, put, ZSTRING_NO_NULL(classref->dataType),
	 classref, sizeof(classref));
  };
};


/** This function initialises the RDF types registry. */
void rdf_init() {
  init_rdf_luts();
  RDFValue_init();
  XSDInteger_init();
  XSDString_init();
  RDFURN_init();
  
  // Register all the known basic types
  register_rdf_value_class((RDFValue)GETCLASS(XSDInteger));
  register_rdf_value_class((RDFValue)GETCLASS(XSDString));
  register_rdf_value_class((RDFValue)GETCLASS(RDFURN));
};

/** RDF parsing */
static void triples_handler(RDFParser self, const raptor_statement* triple) 
{
  const char *urn, *attribute, *value;
  enum resolver_data_type type;

  // Ignore anonymous and invalid triples.
  if(triple->subject_type != RAPTOR_IDENTIFIER_TYPE_RESOURCE ||
     triple->predicate_type != RAPTOR_IDENTIFIER_TYPE_RESOURCE)
    return;

  /* do something with the triple */
#if 0
  urn = (const char *)raptor_uri_as_string(triple->subject);  
  attribute = (const char *)raptor_uri_as_string(triple->predicate);

  switch(triple->object_type) {
  case RAPTOR_IDENTIFIER_TYPE_LITERAL:
  case RAPTOR_IDENTIFIER_TYPE_XML_LITERAL:
    value = triple->object;
    type = RESOLVER_DATA_STRING;
    break;

  case RAPTOR_IDENTIFIER_TYPE_RESOURCE:
    value = (const char *)raptor_uri_as_string(triple->object);
    type = RESOLVER_DATA_URN;
    break;

  default:
    DEBUG("Unable to parse object type %u", triple->object_type);
    return;
  };
  
  CALL(oracle, set, urn, attribute, value, type);
#endif

}

static void message_handler(RDFParser self, raptor_locator* locator, 
			    const char *message)
{
  printf("%s\n", message);
}

static void fatal_error_handler(RDFParser self, raptor_locator *locator,
			  const char *message) { 
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
    int len = CALL(fd, read, buff, BUFF_SIZE);

    if(len<=0) break;
    // Shove it into the parser.
    raptor_parse_chunk(rdf_parser, buff, len, 0);
  }
  // Done - flush the parser
  raptor_parse_chunk(rdf_parser, NULL, 0, 1); /* no data and is_end =
						 1 */
  // Cleanup
  if(uri)
    raptor_free_uri((raptor_uri*)uri);
  raptor_free_parser(rdf_parser);
  return 1;

 error:
  if(uri)
    raptor_free_uri((raptor_uri*)uri);
  raptor_free_parser(rdf_parser);

  return 0;
};

static RDFParser RDFParser_Con(RDFParser self) {
  return self;
};

VIRTUAL(RDFParser, Object)
     VMETHOD(triples_handler) = triples_handler;
     VMETHOD(message_handler) = message_handler;
     VMETHOD(parse) = RDFParser_parse;
     VMETHOD(Con) = RDFParser_Con;
END_VIRTUAL


/*** RDF serialization methods */

static int iostream_write_byte(void *context, const int byte) {
  RDFSerializer self=(RDFSerializer)context;
  char c = byte;

  return CALL(self->fd, write, AS_BUFFER(c));
};

static int iostream_write_bytes(void *context, const void *ptr, size_t size, size_t nmemb) {
  RDFSerializer self=(RDFSerializer)context;
  int length = nmemb * size;

  return CALL(self->fd, write, (char *)ptr, length);
}

raptor_iostream_handler2 raptor_special_handler = {
  .version = 2,
  .write_byte = iostream_write_byte,
  .write_bytes = iostream_write_bytes,
};

static RDFSerializer RDFSerializer_Con(RDFSerializer self, char *base, FileLikeObject fd) {
  char *type = "turtle";

  self->fd = fd;
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
    uri = (void*)raptor_new_uri((const unsigned char*)FQN);
    raptor_serialize_set_namespace(self->rdf_serializer, uri, "");
    raptor_free_uri(uri);
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};


// This function will be run over all the attributes for a given URN
// and we serialise them
static int tdb_attribute_traverse_func(TDB_CONTEXT *tdb, TDB_DATA key, 
				       TDB_DATA value, void *data) {
  RDFSerializer self=(RDFSerializer) data;
  RESOLVER_ITER iter;
  char *attribute;
  raptor_statement triple;

  // Only look at proper attributes
  if(memcmp(key.dptr, "aff4", 4))
    return 0;

  // Skip serializing of volatile triples
  if(!memcmp(key.dptr, ZSTRING_NO_NULL(VOLATILE_NS)))
    return 0;

  memset(&triple, 0, sizeof(triple));

  // NULL terminate the attribute name
  attribute = talloc_size(NULL, key.dsize+1);
  memcpy(attribute, key.dptr, key.dsize);
  attribute[key.dsize]=0;

  // Iterate over all values for this attribute
  CALL(oracle, get_iter, &iter, self->urn, attribute);
  while(1) {
    RDFValue value = CALL(oracle, iter_next_alloc, NULL, &iter);
    char inbuff[BUFF_SIZE];
    char outbuf[BUFF_SIZE];
    char *formatstr = NULL;

    if(!value) break;

    memset(inbuff, 0, BUFF_SIZE);
    lseek(oracle->data_store_fd, iter.offset, SEEK_SET);
    read(oracle->data_store_fd, inbuff, iter.head.length);

    // Now iterate over all the values for this 
    triple.subject = self->raptor_uri;
    triple.subject_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
    
    triple.predicate = (void*)raptor_new_uri((const unsigned char*)attribute);
    triple.predicate_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
    triple.object_type = RAPTOR_IDENTIFIER_TYPE_LITERAL;
    triple.object = outbuf;

    // FIXME
#if 0
    switch(iter.head.encoding_type) {
    case RESOLVER_DATA_URN:
      triple.object = (void*)raptor_new_uri((const unsigned char*)inbuff);
      triple.object_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
      break;
      
    case RESOLVER_DATA_UINT64:
      snprintf(outbuf, BUFF_SIZE, "%llu", *(uint64_t *)inbuff);
      triple.object_literal_datatype = "xsd:long";
     break;
    case RESOLVER_DATA_UINT32:
      snprintf(outbuf, BUFF_SIZE, "%lu", *(uint32_t *)inbuff);
      triple.object_literal_datatype = "xsd:int";
      break;
    case RESOLVER_DATA_UINT16:
      snprintf(outbuf, BUFF_SIZE, "%lu", *(uint16_t *)inbuff);
      triple.object_literal_datatype = "xsd:short";
      break;
    case RESOLVER_DATA_STRING:
      snprintf(outbuf, BUFF_SIZE, "%s", (char *)inbuff);
      break;

    case RESOLVER_DATA_TDB_DATA: {
      char *outbuf = talloc_size(attribute, iter.head.length * 2);
      int length = encode64(inbuff, iter.head.length, outbuf, iter.head.length * 2);

      // Need to escape this data
      triple.object = outbuf;
      triple.object_literal_datatype = "xsd:base64Binary";
      break;
    };
    default:
      RaiseError(ERuntimeError, "Unable to serialize type %d", iter.head.encoding_type);
      goto exit;
    };

    raptor_serialize_statement(self->rdf_serializer, &triple);
#endif

    raptor_free_uri((raptor_uri*)triple.predicate);
    if(triple.object_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE)
      raptor_free_uri((raptor_uri*)triple.object);    

    talloc_free(value);
  };

 exit:
  talloc_free(attribute);
  return 0;
};

static int RDFSerializer_serialize_urn(RDFSerializer self, 
				       RDFURN urn) {
  self->raptor_uri = (void*)raptor_new_uri((const unsigned char*)urn->value);
  self->urn = talloc_strdup(self, urn->value);
  tdb_traverse_read(oracle->attribute_db, tdb_attribute_traverse_func, self);
  raptor_free_uri((raptor_uri*)self->raptor_uri);

  return 1;
};

static void RDFSerializer_close(RDFSerializer self) {
  raptor_serialize_end(self->rdf_serializer);

  raptor_free_serializer(self->rdf_serializer);
  CALL(self->fd, close);
};

VIRTUAL(RDFSerializer, Object)
     VMETHOD(Con) = RDFSerializer_Con;
     VMETHOD(serialize_urn) = RDFSerializer_serialize_urn;
     VMETHOD(close) = RDFSerializer_close;

     raptor_init();
END_VIRTUAL


/*** Convenience functions */
		    // FIXME - this is not thread safe - remove it -
		    // use the new_* functions below.
static XSDInteger integer_cache = NULL;
static XSDString string_cache = NULL;
static RDFURN urn_cache = NULL;

RDFValue rdfvalue_from_int(uint64_t value) {
  if(!integer_cache) 
    integer_cache = CONSTRUCT(XSDInteger, RDFValue, super.Con, NULL);

  return integer_cache->set(integer_cache, value);  
};

RDFValue rdfvalue_from_urn(char *value) {
  if(!urn_cache) 
    urn_cache = CONSTRUCT(RDFURN, RDFValue, super.Con, NULL);

  return urn_cache->set(urn_cache, value);  
};

RDFValue rdfvalue_from_string(char *value) {
  if(!string_cache) 
    string_cache = CONSTRUCT(XSDString, RDFValue, super.Con, NULL);

  return string_cache->set(string_cache, value, strlen(value));  
};

RDFURN new_RDFURN(void *ctx) {
  return CONSTRUCT(RDFURN, RDFValue, super.Con, ctx);
};

XSDInteger new_XSDInteger(void *ctx) {
  return CONSTRUCT(XSDInteger, RDFValue, super.Con, ctx);
};

XSDString new_XSDString(void *ctx) {
  return CONSTRUCT(XSDString, RDFValue, super.Con, ctx);
};
