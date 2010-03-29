/*
** graph.c
** 
** Made by (mic)
** Login   <mic@laptop>
** 
** Started on  Sat Feb 27 01:49:25 2010 mic
** Last update Sun May 12 01:17:25 2002 Speed Blue

This is an implementation of the RDF Graph object

*/

#include "aff4.h"
#include "aff4_rdf_serialise.h"

static AFFObject Graph_Con(AFFObject self, RDFURN url, char mode) {
  Graph this = (Graph)self;

  // Try to parse existing map object
  if(url) {
    URNOF(self) = CALL(url, copy, self);

    this->stored = new_RDFURN(self);
    this->cache = CONSTRUCT(GraphStatement, Cache, Con, self, HASH_TABLE_SIZE, 0);
    this->attribute = new_XSDString(self);

    // Check that we have a stored property
    if(!CALL(oracle, resolve_value, URNOF(self), AFF4_STORED,
	     (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "Object does not have a stored attribute?");
      goto error;
    };

    CALL(oracle, set_value, URNOF(self), AFF4_TYPE, rdfvalue_from_urn(self, AFF4_GRAPH),0);
    CALL(oracle, add_value, this->stored, AFF4_VOLATILE_CONTAINS, (RDFValue)url,0);

    // Ok done
    ClearError();
  } else {
    self = SUPER(AFFObject, FileLikeObject, Con, url, mode);
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};


/** We prepare a statement and store within ourselves */
void Graph_set_triple(Graph self, RDFURN subject, char *attribute, RDFValue value) {
  RESOLVER_ITER *iter = talloc(self, RESOLVER_ITER);
  GraphStatement statement;

  // This should fill in the iterator for later.
  CALL(oracle, set_value, subject, attribute, value, iter);

  // Make a new statement and remember it for later.
  statement = CALL((Cache)self->cache, put, ZSTRING(attribute), (Object)subject);

  statement->iter = iter;
  talloc_steal(statement, iter);
};

static int Graph_close(AFFObject self) {
  Graph this = (Graph)self;
  RDFURN urn = CALL(URNOF(self), copy, self);
  AFF4Volume volume = (AFF4Volume)CALL(oracle, open, this->stored, 'w');
  FileLikeObject segment;
  RDFSerializer serializer;
  Cache i;

  if(!volume) {
    goto error;
  };

  CALL(urn, add, AFF4_INFORMATION "turtle");
  segment = CALL(volume, open_member, urn->value, 'w', ZIP_STORED);
  if(!segment) {
    RaiseError(ERuntimeError, "Unable to create graph segment");
    goto error;
  };

  serializer = CONSTRUCT(RDFSerializer, RDFSerializer, Con, self, STRING_URNOF(volume),
                         segment);
  if(!serializer) goto error;

  list_for_each_entry(i, &((Cache)this->cache)->cache_list, cache_list) {
    GraphStatement statement = (GraphStatement)i;
    char *attribute = i->key;
    RDFURN urn = (RDFURN)i->data;
    RESOLVER_ITER *iter = statement->iter;

    CALL(this->attribute, set, i->key, i->key_len);
    CALL(serializer, serialize_statement, iter, urn, this->attribute);
  };

  CALL(serializer, close);
  CALL((AFFObject)segment, close);
 error:
  if(volume) CALL((AFFObject)volume, cache_return);

  talloc_unlink(NULL, self);
  return 0;
};

VIRTUAL(GraphStatement, Cache)
END_VIRTUAL

VIRTUAL(Graph, AFFObject) {
  VMETHOD(set_triple) = Graph_set_triple;

  VMETHOD_BASE(AFFObject, dataType) = AFF4_GRAPH;
  VMETHOD_BASE(AFFObject, Con) = Graph_Con;
  VMETHOD_BASE(AFFObject, close) = Graph_close;
} END_VIRTUAL

void graph_init() {
  INIT_CLASS(Graph);
  INIT_CLASS(GraphStatement);

  register_type_dispatcher(AFF4_GRAPH, (AFFObject *)GETCLASS(Graph));
};
