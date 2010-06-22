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

// We store these in the resolver as the statements stored within this graph
struct statement_t {
  RESOLVER_ITER iter;
  uint64_t urn_id;
  uint64_t attribute_id;
};

#define AFF4_VOLATILE_GRAPH_STATEMENT VOLATILE_NS "graph_statement"

static AFFObject Graph_Con(AFFObject self, RDFURN url, char mode) {
  Graph this = (Graph)self;

  self = SUPER(AFFObject, FileLikeObject, Con, url, mode);

  // Try to parse existing map object
  if(url) {
    URNOF(self) = CALL(url, copy, self);

    this->stored = new_RDFURN(self);
    this->statement = new_XSDString(self);
    this->attribute_urn = new_RDFURN(self);

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
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};


/** We prepare a statement and store within ourselves */
void Graph_set_triple(Graph self, RDFURN subject, char *attribute, RDFValue value) {
  struct statement_t statement;

  memset(&statement, 0, sizeof(statement));

  // This should fill in the iterator for later.
  CALL(oracle, add_value, subject, attribute, value, &statement.iter);

  statement.urn_id = CALL(oracle, get_id_by_urn, subject, 1);
  CALL(self->attribute_urn, set, attribute);
  statement.attribute_id = CALL(oracle, get_id_by_urn, self->attribute_urn, 1);

  CALL(self->statement, set, (char *)&statement, sizeof(statement));

  CALL(oracle, add_value, URNOF(self), AFF4_VOLATILE_GRAPH_STATEMENT,
       (RDFValue)self->statement, NULL);
};

static int Graph_close(AFFObject self) {
  Graph this = (Graph)self;
  RDFURN urn;
  AFF4Volume volume;
  FileLikeObject segment;
  RDFSerializer serializer;
  Cache i;
  RESOLVER_ITER *iter;

  // Now we iterate over all our stored statements and serialise them
  iter = CALL(oracle, get_iter, self, URNOF(self), AFF4_VOLATILE_GRAPH_STATEMENT);
  // No statements were stored - thats ok but we just dont create a
  // segment at all then
  if(!iter) {
    return 0;
  };

  volume = (AFF4Volume)CALL(oracle, open, this->stored, 'w');
  if(!volume) {
    goto error;
  };

  urn = CALL(URNOF(self), copy, self);
  CALL(urn, add, AFF4_INFORMATION "turtle");
  segment = CALL(volume, open_member, urn->value, 'w', ZIP_STORED);
  if(!segment) {
    RaiseError(ERuntimeError, "Unable to create graph segment");
    goto error;
  };

  serializer = CONSTRUCT(RDFSerializer, RDFSerializer, Con, self, STRING_URNOF(volume),
                         segment);
  if(!serializer) goto error;

  while(CALL(oracle, iter_next, iter, (RDFValue)this->statement)) {
    struct statement_t *statement;

    statement = (struct statement_t *)(this->statement->value);
    statement->iter.cache = NULL;
    statement->iter.urn = NULL;

    if(CALL(oracle, get_urn_by_id, statement->urn_id, urn) &&
       CALL(oracle, get_urn_by_id, statement->attribute_id, this->attribute_urn)) {
      CALL(serializer, serialize_statement, &statement->iter, urn,
           this->attribute_urn);
    };
  };

  CALL(serializer, close);
  CALL((AFFObject)segment, close);
 error:
  if(volume) CALL((AFFObject)volume, cache_return);

  talloc_unlink(NULL, self);
  return 0;
};

VIRTUAL(Graph, AFFObject) {
  VMETHOD(set_triple) = Graph_set_triple;

  VMETHOD_BASE(AFFObject, dataType) = AFF4_GRAPH;
  VMETHOD_BASE(AFFObject, Con) = Graph_Con;
  VMETHOD_BASE(AFFObject, close) = Graph_close;
} END_VIRTUAL

AFF4_MODULE_INIT(graph) {
  register_type_dispatcher(oracle, AFF4_GRAPH, (AFFObject *)GETCLASS(Graph));
};
