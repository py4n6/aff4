/*
** aff4_rdf_serialise.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Wed Nov 18 12:36:21 2009 mic
** Last update Wed Nov 18 12:36:21 2009 mic
*/

#ifndef   	AFF4_RDF_SERIALISE_H_
# define   	AFF4_RDF_SERIALISE_H_

#include "aff4_rdf.h"
#include "aff4_io.h"

/***** Following is an implementation of a serialiser */
CLASS(RDFSerializer, Object)
     raptor_serializer *rdf_serializer;
     raptor_iostream *iostream;
     FileLikeObject fd;

     void *raptor_uri;
     RDFURN urn;

     RDFSerializer METHOD(RDFSerializer, Con, char *base_urn, FileLikeObject fd);
     int METHOD(RDFSerializer, serialize_urn, RDFURN urn);
     void METHOD(RDFSerializer, close);
END_CLASS

CLASS(RDFParser, Object)
     char message[BUFF_SIZE];
     jmp_buf env;

     void METHOD(RDFParser, triples_handler, const raptor_statement *triple);
     void METHOD(RDFParser, message_handler, raptor_locator* locator, const char *message);

// Parses data stored in fd using the format specified. fd is assumed
// to contain a base URN specified (or NULL if non specified).
     int METHOD(RDFParser, parse, FileLikeObject fd, char *format, char *base);
     RDFParser METHOD(RDFParser, Con);
END_CLASS

#endif 	    /* !AFF4_RDF_SERIALISE_H_ */
