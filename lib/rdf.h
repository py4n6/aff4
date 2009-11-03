#include "class.h"
#include <raptor.h>
#include <setjmp.h>

struct FileLikeObject;

/***** Following is an implementation of a serialiser */
CLASS(RDFSerializer, Object)
     raptor_serializer *rdf_serializer;
     raptor_iostream *iostream;
     FileLikeObject fd;

     void *raptor_uri;
     char *urn;

     RDFSerializer METHOD(RDFSerializer, Con, char *base_urn, struct FileLikeObject *fd);
     int METHOD(RDFSerializer, serialize_urn, char *urn_str);
     void METHOD(RDFSerializer, close);
END_CLASS
