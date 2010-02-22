
#ifndef   	AFF4_RDF_H_
# define   	AFF4_RDF_H_

#include "class.h"
#include <tdb.h>
#include <raptor.h>
#include <setjmp.h>
#include "misc.h"
#include "aff4_utils.h"
#include <sys/time.h>

struct RDFURN_t;

/**** A class used to parse URNs */
CLASS(URLParse, Object)
      char *scheme;
      char *netloc;
      char *query;
      char *fragment;

      void *ctx;

      URLParse METHOD(URLParse, Con, char *url);

      // Parses the url given and sets internal attributes.
      int METHOD(URLParse, parse, char *url);

      // Returns the internal attributes joined together into a valid URL.
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

      // These flags will be stored in the TDB_DATA_LIST and specify
      // our behaviour - used for optimizations.
      uint8_t flags;

      // This is only required for special handling - Leave as NULL to
      // be the same as dataType above
      raptor_uri *raptor_literal_datatype;

      RDFValue METHOD(RDFValue, Con);

      /* This method is called to parse a serialised form into this
	 instance. Return 1 if parsing is successful, 0 if error
	 occured.
      */
      int METHOD(RDFValue, parse, char *serialised_form, struct RDFURN_t *subject);

      /* This method is called to serialise this object into the
	 TDB_DATA struct for storage in the TDB data store. The new
	 memory will be allocated with this object's context and must
	 be freed by the caller.
      */
      TDB_DATA *METHOD(RDFValue, encode, struct RDFURN_t *subject);

      // This method is used to decode this object from the
      // data_store. The fd is seeked to the start of this record.
      int METHOD(RDFValue, decode, char *data, int length, struct RDFURN_t *subject);

      /** This method will serialise the value into a null terminated
	  string for export into RDF. The returned string will be
	  allocated to the NULL context and should be unlinked by the caller.
      */
      char *METHOD(RDFValue, serialise, struct RDFURN_t *subject);
END_CLASS

      /** The following is a direction for the autogenerator to create
          a proxied class for providing a python class as an
          implementation of RDFValue.
      */
PROXY_CLASS(RDFValue)


      /** An integer encoded according the XML RFC. */
CLASS(XSDInteger, RDFValue)
     int64_t value;
     char *serialised;

     void METHOD(XSDInteger, set, uint64_t value);
     uint64_t METHOD(XSDInteger, get);
END_CLASS

     /** A literal string */
CLASS(XSDString, RDFValue)
     char *value;
     int length;

     void METHOD(XSDString, set, char *string, int length);
     BORROWED char *METHOD(XSDString, get);
END_CLASS

     /** Dates serialised according the XML standard */
CLASS(XSDDatetime, RDFValue)
     struct timeval value;
     int gm_offset;
     char *serialised;

     void METHOD(XSDDatetime, set, struct timeval time);
END_CLASS

     /** A URN for use in the rest of the library */
CLASS(RDFURN, RDFValue)
     char *value;

     // This parser maintains our internal state
     URLParse parser;

     void METHOD(RDFURN, set, char *urn);
     BORROWED char *METHOD(RDFURN, get);

     // Make a new RDFURN as a copy of this one
     RDFURN METHOD(RDFURN, copy, void *ctx);

     // Add a relative stem to the current value. If urn is a fully
     // qualified URN, we replace the current value with it
     void METHOD(RDFURN, add, char *urn);

     /** This adds the binary string in filename into the end of the
     URL query string, escaping invalid characters.
     */
     void METHOD(RDFURN, add_query, unsigned char *query, unsigned int len);

     // This method returns the relative name
     TDB_DATA METHOD(RDFURN, relative_name, RDFURN volume);
END_CLASS


     /** An integer array stores an array of integers
         efficiently. This variant stores it in a binary file.
     */
CLASS(IntegerArrayBinary, RDFValue)
     uint32_t *array;

  // Current pointer for adding members
  int current;

  // The total number of indexes in this array
  int size;

  // The available allocated memory (we allocate chunks of BUFF_SIZE)
  int alloc_size;

  // The urn of the array is derived from the subject by appending
  // this extension.
  char *extension;

  void METHOD(IntegerArrayBinary, add, unsigned int offset);
END_CLASS


  /** This is used when the array is fairly small and it can fit
      inline */
CLASS(IntegerArrayInline, IntegerArrayBinary)
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
