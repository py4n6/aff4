/** This handles the RDF serialization, and parsing from the
    resolver.
*/
/** The following are related to raptor - RDF serialization and
    parsing
*/
#include "aff4.h"
#include "aff4_rdf_serialise.h"
#include "aff4_rdf.h"
#include <time.h>

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
  if(url)
    CALL(self, parse ,url);
  self->ctx = talloc_size(self, 1);
  return self;
};

int URLParse_parse(URLParse self, char *url) {
  int i=0,j=0,end=0;
  char buff[BUFF_SIZE];
  enum url_state state = STATE_METHOD;

  talloc_free(self->ctx);
  self->ctx = talloc_size(self, 1);

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
      if(url[i]==0) {
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
      if(url[i]==0 || (url[i]=='/' && i--)) {
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

char *URLParse_string(URLParse self, void *ctx) {
  char *fmt;
  char *scheme = self->scheme;
  char *seperator = "/";

  if(strlen(self->fragment)>0)
    fmt = "%s://%s%s%s#%s";
  else if(strlen(self->query)>0) {
    if(self->query[0] == '/') seperator = "";
    fmt = "%s://%s%s%s";
  } else
    fmt = "%s://%s";

  if(strlen(scheme)==0)
    scheme = "file";

  return talloc_asprintf(ctx, fmt, scheme,
			 self->netloc, seperator, 
                         self->query,
			 self->fragment);
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

VIRTUAL(RDFValue, Object) {
    VMETHOD(Con) = RDFValue_Con;
} END_VIRTUAL

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

static int XSDInteger_decode(RDFValue this, char *data, int length) {
  XSDInteger self = (XSDInteger)this;

  self->value = 0;
  memcpy(&self->value, data, sizeof(self->value));
  return length;
};

static void *XSDInteger_serialise(RDFValue self) {
  XSDInteger this = (XSDInteger)self;

  if(this->serialised) 
    talloc_free(this->serialised);

  this->serialised = talloc_asprintf(self, "%llu", this->value);
  
  return this->serialised;
};

static int XSDInteger_parse(RDFValue self, char *serialised) {
  XSDInteger this = (XSDInteger)self;

  this->value = strtoll(serialised, NULL, 0);

  return 1;
};

VIRTUAL(XSDInteger, RDFValue) {
   VATTR(super.raptor_type) = RAPTOR_IDENTIFIER_TYPE_LITERAL;

   // This is our official data type - or at least what raptor gives us
   VATTR(super.dataType) = "http://www.w3.org/2001/XMLSchema#integer";
   VATTR(super.raptor_literal_datatype) = raptor_new_uri(		\
		   (const unsigned char*)VATTR(super.dataType));

   VMETHOD(super.encode) = XSDInteger_encode;
   VMETHOD(super.decode) = XSDInteger_decode;
   VMETHOD(super.serialise) = XSDInteger_serialise;
   VMETHOD(super.parse) = XSDInteger_parse;
   
   VMETHOD(set) = XSDInteger_set;
} END_VIRTUAL

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

static int XSDString_decode(RDFValue this, char *data, int length) {
  XSDString self = (XSDString)this;

  self->value = talloc_realloc_size(self, self->value, length + 1);
  self->length = length;

  self->value[length]=0;
  memcpy(self->value, data, length);

  return length;

};

/* We serialise the string */
static void *XSDString_serialise(RDFValue self) {
  XSDString this = (XSDString)self;
  
  return this->value;
};

static int XSDString_parse(RDFValue self, char *serialise) {
  XSDString this = (XSDString)self;

  this->value = talloc_strdup(self, serialise);
  this->length = strlen(this->value);

  return 1;
};

VIRTUAL(XSDString, RDFValue) {
   VATTR(super.raptor_type) = RAPTOR_IDENTIFIER_TYPE_LITERAL;
   VATTR(super.dataType) = XSD_NAMESPACE "string";

   VMETHOD(super.encode) = XSDString_encode;
   VMETHOD(super.decode) = XSDString_decode;
   VMETHOD(super.serialise) = XSDString_serialise;
   VMETHOD(super.parse) = XSDString_parse;

   VMETHOD(set) = XSDString_set;
} END_VIRTUAL 

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
  CALL(self->parser, parse, string);

  // Our value is the serialsed version of what the parser got:
  if(self->value) talloc_free(self->value);
  self->value = CALL(self->parser, string, self);

  return (RDFValue)self;
};

static int RDFURN_decode(RDFValue this, char *data, int length) {
  RDFURN self = (RDFURN)this;
  self->value = talloc_realloc_size(self, self->value, length+1);
  self->value[length]=0;

  memcpy(self->value, data, length);
  CALL(self->parser, parse, self->value);

  return length;
};

/* We serialise the string */
static void *RDFURN_serialise(RDFValue self) {
  RDFURN this = (RDFURN)self;

  if(this->serialised) talloc_free(this->serialised);
  this->serialised = raptor_new_uri((const unsigned char*)this->value);

  return this->serialised;
};

static RDFURN RDFURN_copy(RDFURN self, void *ctx) {
  RDFURN result = CONSTRUCT(RDFURN, RDFValue, super.Con, ctx);

  CALL(result, set, self->value);

  return result;
};

static int RDFURN_parse(RDFValue self, char *serialised) {
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
    result.dptr = (unsigned char *)self->value + strlen(volume->value) + 1;
    result.dsize = strlen((char *)result.dptr);
  } else {
    result = tdb_data_from_string(self->value);
  };

  return result;
};

VIRTUAL(RDFURN, RDFValue) {
   VATTR(super.raptor_type) = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
   VATTR(super.dataType) = "rdf:urn";

   VMETHOD(super.Con) = RDFURN_Con;
   VMETHOD(super.encode) = RDFURN_encode;
   VMETHOD(super.decode) = RDFURN_decode;
   VMETHOD(super.serialise) = RDFURN_serialise;
   VMETHOD(super.parse) = RDFURN_parse;

   VMETHOD(set) = RDFURN_set;
   VMETHOD(add) = RDFURN_add;
   VMETHOD(copy) = RDFURN_copy;
   VMETHOD(relative_name) = RDFURN_relative_name;
} END_VIRTUAL

static TDB_DATA *XSDDatetime_encode(RDFValue self) {
  XSDDatetime this = (XSDDatetime)self;
  TDB_DATA *result = talloc(self, TDB_DATA);

  result->dptr = (unsigned char *)&this->value;
  result->dsize = sizeof(this->value);

  return result;
};

static int XSDDatetime_decode(RDFValue self, char *data, int length) {
  XSDDatetime this = (XSDDatetime)self;

  memcpy(&this->value, data, sizeof(this->value));
  return length;
};

static RDFValue XSDDatetime_set(XSDDatetime self, struct timeval time) {
  self->value = time;

  return (RDFValue)self;
};

#define DATETIME_FORMAT_STR "%Y-%m-%dT%H:%M:%S"

static void *XSDDatetime_serialise(RDFValue self) {
  XSDDatetime this = (XSDDatetime)self;
  char buff[BUFF_SIZE];

  if(this->serialised) talloc_free(this->serialised);

  if(BUFF_SIZE > strftime(buff, BUFF_SIZE, DATETIME_FORMAT_STR,
			  localtime(&this->value.tv_sec))) {
    this->serialised = talloc_asprintf(this, "%s.%06u+%02u:%02u", buff,
                                       0,
				       //(unsigned int)this->value.tv_usec,
				       (unsigned int)this->tz.tz_minuteswest / 60,
				       (unsigned int)this->tz.tz_minuteswest % 60);
    return this->serialised;
  };

  return NULL;
};

static int XSDDatetime_parse(RDFValue self, char *serialised) {
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
   VATTR(super.raptor_type) = RAPTOR_IDENTIFIER_TYPE_LITERAL;
   VATTR(super.dataType) = XSD_NAMESPACE "dateTime";
   VATTR(super.raptor_literal_datatype) = raptor_new_uri(			\
                    (const unsigned char*)VATTR(super.dataType));

   VMETHOD(super.encode) = XSDDatetime_encode;
   VMETHOD(super.decode) = XSDDatetime_decode;
   VMETHOD(super.serialise) = XSDDatetime_serialise;
   VMETHOD(super.parse) = XSDDatetime_parse;
   VMETHOD(super.Con) = XSDDatetime_Con;

   VMETHOD(set) = XSDDatetime_set;
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
  Cache tmp;

  if(!RDF_Registry) {
    RDF_Registry = CONSTRUCT(Cache, Cache, Con, NULL, 100, 0);
    RDF_Registry->static_objects = 1;
    talloc_set_name_const(RDF_Registry, "RDFValue dispatcher");
  };

  tmp = CALL(RDF_Registry, get, ZSTRING_NO_NULL(classref->dataType));
  if(!tmp) {
    Cache tmp =  CALL(RDF_Registry, put, ZSTRING_NO_NULL(classref->dataType),
		      classref, sizeof(classref));
    talloc_set_name(tmp, "RDFValue type %s", NAMEOF(classref));
  };
};


/** This function initialises the RDF types registry. */
void rdf_init() {
  raptor_init();
  RDFValue_init();
  XSDInteger_init();
  XSDString_init();
  RDFURN_init();
  XSDDatetime_init();
  
  // Register all the known basic types
  register_rdf_value_class((RDFValue)GETCLASS(XSDInteger));
  register_rdf_value_class((RDFValue)GETCLASS(XSDString));
  register_rdf_value_class((RDFValue)GETCLASS(RDFURN));
  register_rdf_value_class((RDFValue)GETCLASS(XSDDatetime));
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
    char *name = CALL(self->member_cache, get_item, ZSTRING_NO_NULL(urn_str));

    if(!name) {
      // Make sure the volume contains this object
      printf("!!! %s contains %s\n", self->volume_urn->value, urn_str);

      CALL(oracle, add_value, self->volume_urn, AFF4_VOLATILE_CONTAINS,
           (RDFValue)self->urn);

      CALL(self->member_cache, put, ZSTRING_NO_NULL(urn_str), ZSTRING_NO_NULL(urn_str));
    };
  };

  if(triple->object_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE) {
    class_ref = (RDFValue)GETCLASS(RDFURN);
  } else if(triple->object_literal_datatype) {
    class_ref = CALL(RDF_Registry, get_item, 
		     ZSTRING_NO_NULL(type_str));
  };

  result = CONSTRUCT_FROM_REFERENCE(class_ref, Con, NULL);
  if(result) {
    CALL(result, parse, value_str);
    CALL(oracle, add_value, self->urn, attribute, (RDFValue)result);
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
  self->urn = new_RDFURN(self);
  self->volume_urn = new_RDFURN(self);
  self->member_cache = CONSTRUCT(Cache, Cache, Con, self, 100, 0);
  self->member_cache->static_objects = 1;
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
    uri = (void*)raptor_new_uri((const unsigned char*)PREDICATE_NAMESPACE);
    raptor_serialize_set_namespace(self->rdf_serializer, uri, (unsigned char *)"aff4");
    raptor_free_uri(uri);

    uri = (void*)raptor_new_uri((const unsigned char*)XSD_NAMESPACE);
    raptor_serialize_set_namespace(self->rdf_serializer, uri, (unsigned char *)"xsd");
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
  RESOLVER_ITER *iter;
  char *attribute;
  raptor_statement triple;

  // Only look at proper attributes
  if(key.dptr[0]=='_')
    goto exit;

  // Skip serializing of volatile triples
  if(!memcmp(key.dptr, ZSTRING_NO_NULL(VOLATILE_NS)))
    goto exit;

  memset(&triple, 0, sizeof(triple));

  // NULL terminate the attribute name
  attribute = talloc_size(NULL, key.dsize+1);
  memcpy(attribute, key.dptr, key.dsize);
  attribute[key.dsize]=0;

  // Iterate over all values for this attribute
  iter = CALL(oracle, get_iter, attribute, self->urn, attribute);
  while(1) {
    RDFValue value = CALL(oracle, iter_next_alloc, iter);

    if(!value) break;

    // Now iterate over all the values for this
    triple.subject = self->raptor_uri;
    triple.subject_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;

    triple.predicate = (void*)raptor_new_uri((const unsigned char*)attribute);
    triple.predicate_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
    triple.object_type = value->raptor_type;
    triple.object = CALL(value, serialise);
    triple.object_literal_datatype = value->raptor_literal_datatype;

    raptor_serialize_statement(self->rdf_serializer, &triple);
    raptor_free_uri((raptor_uri*)triple.predicate);

    if(triple.object_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE)
      raptor_free_uri((raptor_uri*)triple.object);

  };

  talloc_free(attribute);
 exit:
  return 0;
};

static int RDFSerializer_serialize_urn(RDFSerializer self, 
				       RDFURN urn) {
  self->raptor_uri = (void*)raptor_new_uri((const unsigned char*)urn->value);
  self->urn = urn;
  tdb_traverse_read(oracle->attribute_db, tdb_attribute_traverse_func, self);
  raptor_free_uri((raptor_uri*)self->raptor_uri);
  self->urn = NULL;

  return 1;
};

static void RDFSerializer_close(RDFSerializer self) {
  raptor_serialize_end(self->rdf_serializer);

  raptor_free_serializer(self->rdf_serializer);
  CALL(self->fd, close);
};

VIRTUAL(RDFSerializer, Object) {
     VMETHOD(Con) = RDFSerializer_Con;
     VMETHOD(serialize_urn) = RDFSerializer_serialize_urn;
     VMETHOD(close) = RDFSerializer_close;
} END_VIRTUAL


/*** Convenience functions */
RDFValue rdfvalue_from_int(void *ctx, uint64_t value) {
  XSDInteger result = new_XSDInteger(ctx);

  return result->set(result, value);  
};

RDFValue rdfvalue_from_urn(void *ctx, char *value) {
  RDFURN result = new_RDFURN(ctx);
  if(!value) return NULL;

  return result->set(result, value);  
};

RDFValue rdfvalue_from_string(void *ctx, char *value) {
  XSDString result = CONSTRUCT(XSDString, RDFValue, super.Con, ctx);

  result->set(result, value, strlen(value));

  return (RDFValue)result;
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

XSDDatetime new_XSDDateTime(void *ctx) {
  return CONSTRUCT(XSDDatetime, RDFValue, super.Con, ctx);
};
