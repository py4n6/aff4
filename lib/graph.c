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

void Graph_set_triple(Graph self, RDFURN subject, char *attribute, RDFValue value) {
  Graph_add_value(URNOF(self), subject, attribute, value);
};

VIRTUAL(Graph, FileLikeObject) {
  VMETHOD(set_triple) = Graph_set_triple;
} END_VIRTUAL
