
#ifndef   	AFF4_RDF_H_
# define   	AFF4_RDF_H_

#include "class.h"
#include <tdb.h>
#include <raptor.h>
#include <setjmp.h>
#include "misc.h"
#include "aff4_utils.h"
#include <sys/time.h>

/**** A class used to parse URNs */
CLASS(URLParse, Object)
      char *scheme;
      char *netloc;
      char *query;
      char *fragment;

      void *ctx;

      URLParse METHOD(URLParse, Con, char *url);
      int METHOD(URLParse, parse, char *url);
      char *METHOD(URLParse, string, void *ctx);
END_CLASS

/***** The RDF resolver stores objects inherited from this one.

       You can define other objects and register them using the
       register_rdf_value_class() function. You will need to extend
       this base class and initialise it with a unique dataType URN.

       RDFValue is the base class for all other values.
******/
CLASS(RDFValue, Object)
      char *dataType;
      int id;
      raptor_identifier_type raptor_type;

      // This is only required for special handling
      raptor_uri *raptor_literal_datatype;

      RDFValue METHOD(RDFValue, Con);

      /* This method is called to parse a serialised form into this
	 instance. Return 1 if parsing is successful, 0 if error
	 occured. 
      */
      int METHOD(RDFValue, parse, char *serialised_form);

      /* This method is called to serialise this object into the
	 TDB_DATA struct for storage in the TDB data store. The new
	 memory will be allocated with this object's context and must
	 be freed by the caller.
      */
      TDB_DATA *METHOD(RDFValue, encode);

      // This method is used to decode this object from the
      // data_store. The fd is seeked to the start of this record.
      int METHOD(RDFValue, decode, char *data, int length);

      /** This method will serialise the value into a null terminated
	  string for export into RDF. The returned string will be
	  allocated internally and should not be freed by the caller. 
      */
      BORROWED char *METHOD(RDFValue, serialise);
END_CLASS

      /** An integer */
CLASS(XSDInteger, RDFValue)
     int64_t value;
     char *serialised;

     RDFValue METHOD(XSDInteger, set, uint64_t value);
     uint64_t METHOD(XSDInteger, get);
END_CLASS

     /** A literal (string) */
CLASS(XSDString, RDFValue)
     char *value;
     int length;

     RDFValue METHOD(XSDString, set, char *string, int length);
     char *METHOD(XSDString, get);
END_CLASS

CLASS(XSDDatetime, RDFValue)
     struct timeval value;
     struct timezone tz;
     char *serialised;

     BORROWED RDFValue METHOD(XSDDatetime, set, struct timeval time);
END_CLASS

CLASS(RDFURN, RDFValue)
     char *value;

     void *serialised;

     // This parser maintains our internal state
     URLParse parser;

     RDFValue METHOD(RDFURN, set, char *urn);
     char *METHOD(RDFURN, get);

     // Make a new RDFURN as a copy of this one
     RDFURN METHOD(RDFURN, copy, void *ctx);

     // Add a relative stem to the current value. If uri is a fully
     // qualified URN, we replace the current value with it
     void METHOD(RDFURN, add, char *urn);

     // This method returns the relative name 
     TDB_DATA METHOD(RDFURN, relative_name, RDFURN volume);
END_CLASS

     /** This function is used to register a new RDFValue class with
	 the RDF subsystem. It can then be serialised, and parsed.
	 
	 Prior to calling this function you need to initialise the
	 class because we will use its dataType attribute.
     **/
void register_rdf_value_class(RDFValue class_ref);
extern Cache RDF_Registry;

     /** The following are convenience functions that allow easy
	 access to some common types.
     */
RDFValue rdfvalue_from_int(void *ctx, uint64_t value);
RDFValue rdfvalue_from_urn(void *ctx, char *value);
RDFValue rdfvalue_from_string(void *ctx, char *value);

RDFURN new_RDFURN(void *ctx);
XSDInteger new_XSDInteger(void *ctx);
XSDString new_XSDString(void *ctx);
XSDDatetime new_XSDDateTime(void *ctx);

// Generic constructor for an RDFValue
RDFValue new_rdfvalue(void *ctx, char *type);

void rdf_init();

#endif
