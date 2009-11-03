/** This handles the RDF serialization, and parsing from the
    resolver. 
*/
/** The following are related to raptor - RDF serialization and
    parsing 
*/
#include "zip.h"
#include "rdf.h"

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

  CALL(oracle, get_iter, &iter, self->urn, attribute, RESOLVER_DATA_ANY);
  while(CALL(oracle, iter_next, &iter, NULL, 0)) {
    char inbuff[BUFF_SIZE];
    char outbuf[BUFF_SIZE];
    char *formatstr = NULL;

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
    
    switch(iter.head.encoding_type) {
    case RESOLVER_DATA_URN:
      triple.object = (void*)raptor_new_uri((const unsigned char*)inbuff);
      triple.object_type = RAPTOR_IDENTIFIER_TYPE_RESOURCE;
      break;
      
    case RESOLVER_DATA_UINT64:
      snprintf(outbuf, BUFF_SIZE, "%llu", *(uint64_t *)inbuff);
      break;
    case RESOLVER_DATA_UINT32:
      snprintf(outbuf, BUFF_SIZE, "%lu", *(uint32_t *)inbuff);
      break;
    case RESOLVER_DATA_UINT16:
      snprintf(outbuf, BUFF_SIZE, "%lu", *(uint16_t *)inbuff);
      break;
    case RESOLVER_DATA_STRING:
      snprintf(outbuf, BUFF_SIZE, "%s", (char *)inbuff);
      break;

    case RESOLVER_DATA_TDB_DATA: {
      // Need to escape this data
      triple.object = escape_filename(attribute, inbuff, iter.head.length);
      break;
    };
    default:
      RaiseError(ERuntimeError, "Unable to serialize type %d", iter.head.encoding_type);
      goto exit;
    };

    raptor_serialize_statement(self->rdf_serializer, &triple);

    raptor_free_uri((raptor_uri*)triple.predicate);
    if(triple.object_type == RAPTOR_IDENTIFIER_TYPE_RESOURCE)
      raptor_free_uri((raptor_uri*)triple.object);    
  };

 exit:
  talloc_free(attribute);
  return 0;
};

static int RDFSerializer_serialize_urn(RDFSerializer self, 
				       char *urn_str) {
  self->raptor_uri = (void*)raptor_new_uri((const unsigned char*)urn_str);
  self->urn = urn_str;
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
