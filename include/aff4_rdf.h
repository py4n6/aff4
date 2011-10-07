
#ifndef   	AFF4_RDF_H_
# define   	AFF4_RDF_H_

struct RDFURN_t;

struct DataStoreObject_t;
struct Resolver_t;


/**** A class used to parse URNs */
CLASS(URLParse, Object)
      char *scheme;
      char *netloc;
      char *query;
      char *fragment;

      void *ctx;

      URLParse METHOD(URLParse, Con, char *url);

      /* Parses the url given and sets internal attributes. */
      int METHOD(URLParse, parse, char *url);

      /* Returns the internal attributes joined together into a valid
         URL.
      */
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

      /* This is only required for special handling - Leave as NULL to
         be the same as dataType above
      */
      raptor_uri *raptor_literal_datatype;

      /* RDFValues can be strung into lists. */
      struct list_head list;

      RDFValue METHOD(RDFValue, Con);

      /* This method is called to parse a serialised form into this
	 instance. Return 1 if parsing is successful, 0 if error
	 occured.

         DEFAULT(subject) = NULL;
      */
      int METHOD(RDFValue, parse, char *serialised_form, \
                 struct RDFURN_t *subject);

      /* This method is called to serialise this object into the data store. The
	 new memory will be allocated with this object's context.
      */
      struct DataStoreObject_t *METHOD(RDFValue, encode, \
                                       struct RDFURN_t *subject, \
                                       struct Resolver_t *resolver);

      /* This method is used to decode this object from the
         data_store. The fd is seeked to the start of this record.
      */
      int METHOD(RDFValue, decode, struct DataStoreObject_t *obj, \
                 struct RDFURN_t *subject, struct Resolver_t *resolver);

      /** This method will serialise the value into a null terminated
	  string for export into RDF. The returned string will be
	  allocated to the NULL context and should be unlinked by the caller.

          DEFAULT(subject) = NULL;
      */
      char *METHOD(RDFValue, serialise, void *ctx, struct RDFURN_t *subject);

      /** Makes a copy of this value - the copy should be made as
          efficiently as possible (It might just take a reference to
          memory instead of copying it).
      */
      RDFValue METHOD(RDFValue, clone, void *ctx);
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

     /** A convenience Constructor to set a new value.

      DEFAULT(value) = 0;
     */
     XSDInteger METHOD(XSDInteger, Con, uint64_t value);

     void METHOD(XSDInteger, set, uint64_t value);
     uint64_t METHOD(XSDInteger, get);
END_CLASS

     /** A literal string */
CLASS(XSDString, RDFValue)
     char *value;
     int length;

     /** A convenience Constructor to set a new value.

         DEFAULT(string) = ""
      */
     XSDString METHOD(XSDString, Con, char *string, int length);

     void METHOD(XSDString, set, char *string, int length);
     BORROWED char *METHOD(XSDString, get);
END_CLASS

/** Dates serialised according the XML standard. Internally we store time
 * since the epoch in milliseconds.
 */
CLASS(XSDDatetime, XSDInteger)
     int gm_offset;
     char *serialised;

     void METHOD(XSDDatetime, set, struct timeval time);
END_CLASS

     /** A URN for use in the rest of the library */
CLASS(RDFURN, RDFValue)
/** This is a normalised string version of the currently set URL. Note
    that we obtain this by parsing the URL and then serialising it -
    so this string is a normalized URL.
 */
     char *value;

     /* This parser maintains our internal state. We use it to parse
        elements.
     */
     URLParse parser;

     /* A convenience constructor.

        DEFAULT(urn) = NULL;
      */
     RDFURN METHOD(RDFURN, Con, char *urn);

     void METHOD(RDFURN, set, char *urn);

     /* Make a new RDFURN as a copy of this one */
     RDFURN METHOD(RDFURN, copy, void *ctx);

     /* Add a relative stem to the current value. If urn is a fully
        qualified URN, we replace the current value with it.
     */
     void METHOD(RDFURN, add, char *urn);

     /** This adds the binary string in filename into the end of the
         URL query string, escaping invalid characters.
     */
     void METHOD(RDFURN, add_query, unsigned char *query, unsigned int len);

     /* This method returns the relative name */
     char *METHOD(RDFURN, relative_name, RDFURN volume);
END_CLASS


     /** An integer array stores an array of integers
         efficiently. This variant stores it in a binary file.
     */
CLASS(IntegerArrayBinary, RDFValue)
     uint32_t *array;

     /* Current pointer for adding members */
     int current;

     /* The total number of indexes in this array */
     int size;

     /* The available allocated memory (we allocate chunks of BUFF_SIZE) */
     int alloc_size;

     /* The urn of the array is derived from the subject by appending
        this extension. */
     char *extension;

     /** Add a new integer to the array */
     void METHOD(IntegerArrayBinary, add, unsigned int offset);
END_CLASS


     /** This is used when the array is fairly small and it can fit
         inline */
CLASS(IntegerArrayInline, IntegerArrayBinary)
END_CLASS

/** This function is used to register a new RDFValue class with
    the RDF subsystem. It can then be serialised, and parsed.

    This function should not be called from users, please call the
    Resolver.register_rdf_value_class() function instead.
**/
void register_rdf_value_class(RDFValue class_ref);

/** The following are convenience functions that allow easy
    access to some common types.

    FIXME - remove these.
*/
RDFValue rdfvalue_from_int(void *ctx, uint64_t value);
RDFValue rdfvalue_from_urn(void *ctx, char *value);
RDFValue rdfvalue_from_string(void *ctx, char *value);

DLL_PUBLIC RDFURN new_RDFURN(void *ctx);
DLL_PUBLIC XSDInteger new_XSDInteger(void *ctx);
DLL_PUBLIC XSDString new_XSDString(void *ctx);
DLL_PUBLIC XSDDatetime new_XSDDateTime(void *ctx);

// Generic constructor for an RDFValue
DLL_PUBLIC RDFValue new_rdfvalue(void *ctx, char *type);
#endif
