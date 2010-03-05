/*
** aff4_resolver.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Thu Nov 12 20:41:24 2009 mic
** Last update Mon Feb  1 15:24:54 2010 mic
*/

#ifndef   	AFF4_RESOLVER_H_
# define   	AFF4_RESOLVER_H_

#include "aff4_io.h"

struct SecurityProvider_t;

extern int AFF4_TDB_FLAGS;

/* The TDB_DATA_LIST flags */
/* This signals that the RDFValue entry has the same encoding and
   serialised form. This allows us to do some optimizations.
*/
#define RESOLVER_ENTRY_ENCODED_SAME_AS_SERIALIZED 1

// The data store is basically a singly linked list of these records:
typedef struct TDB_DATA_LIST {
  uint64_t next_offset;
  uint16_t length;
  // This is the urn_id of the object which asserted this
  // statement. Can be 0 to indicate unknown.
  int32_t asserter_id;

  // This type refers to the full name type as stored in types.tdb
  uint8_t encoding_type;

  // This field contains various flags about this entry
  uint8_t flags;
}__attribute__((packed)) TDB_DATA_LIST;

/** This object is returned when iterating a result set from the
    resolver. Its basically a pointer into the resolver data store.*/
BOUND typedef struct RESOLVER_ITER {
  TDB_DATA_LIST head;
  uint64_t offset;
  // This is used to ensure we do not iterate over multiple values
  // which are the same
  Cache cache;
  RDFURN urn;
} RESOLVER_ITER;

/** The resolver is at the heart of the AFF4 specification - it is
    responsible for returning objects keyed by attribute from a
    globally unique identifier (URI) and managing the central
    information store.
*/
CLASS(Resolver, Object)
// This is a global cache of URN and their values - we try to only
// have small URNs here and keep everything in memory.
       struct tdb_context *urn_db;
       struct tdb_context *attribute_db;
       struct tdb_context *data_db;

       int data_store_fd;
       int mode;

       /** This is used to restore state if the RDF parser fails */
       jmp_buf env;
       char *message;

       // Read and write caches. These have different policies. The
       // read cache is just for efficiency - if an object is not in
       // the cache or is used by another thread, we just create a new
       // one of those.
       Cache read_cache;

       // Write cache is used for locks - it is not possible to have
       // multiple write objects at the same time, and all writers are
       // opened exclusively.
       Cache write_cache;

       // This mutex protects our internal data structures
       pthread_mutex_t mutex;

       // Resolvers contain the identity behind them (see below):
       struct Identity_t *identity;

       // This is used to check the type of new objects
       RDFURN type;

       /* If this is set objects will use this to send messgaes
          through */
       Logger logger;

       /** DEFAULT(mode) = 0; */
       Resolver METHOD(Resolver, Con, int mode);

       // Resolvers are all in a list. Each resolver in the list is another
       // identity which can be signed.
       struct list_head identities;

/* This method tries to resolve the provided uri and returns an
 instance of whatever the URI refers to (As an AFFObject which is the
 common base class. You should check to see that what you get back is
 actually what you need. For example:

 FileLikeObject fd = (FileLikeObject)CALL(resolver, resolve, uri);
 if(!fd || !ISSUBCLASS(fd, FileLikeObject)) goto error;
 
 Once the resolver provides the object it is attached to the context
 ctx and removed from the cache. This ensures that the object can not
 expire from the cache while callers are holding it. You must return
 the object to the cache as soon as possible by calling
 cache_return. The object will be locked until you return it with
 cache_return. 

 DEFAULT(mode) = "r"
*/
      AFFObject METHOD(Resolver, open, RDFURN uri, char mode);

       /* All objects obtained from Resolver.open() need to be
          returned to the cache promptly using this method. NOTE - it
          is an error to be using an object after being returned to
          the cache - it might not be valid and may be gced.

          NOTE for Python bindings - do not use this function, instead
          use AFFObject.cache_return() method which ensures the
          wrapped python object is properly destroyed after this call.
       */
      void METHOD(Resolver, cache_return, AFFObject obj);

       /* This create a new object of the specified type.

          name specifies the type of object as registered in the type
          handler dispatcher. (e.g. AFF4_ZIP_VOLUME)

          DEFAULT(mode) = "w"
        */
       AFFObject METHOD(Resolver, create, char *name, char mode);

  /* This function resolves the value in uri and attribute and sets it
     into the RDFValue object. So you must first create such an object
     (e.g. XSDDatetime) and then pass the object here to be updated
     from the data store. Note that only a single value is returned -
     if you want to iterate over all the values for this attribute you
     need to call get_iter and iter_next.
  */
  int METHOD(Resolver, resolve_value, RDFURN uri, char *attribute,\
	     RDFValue value);

       /** Similar to Resolver.resolve_value, but a new RDFValue is
           allocated with the context provided. */
   RDFValue METHOD(Resolver, resolve_alloc, void *ctx, RDFURN uri, char *attribute);

  /* This is a version of the above which uses an iterator to iterate
     over the list.

     The iterator is obtained using get_iter first. This function
     returns 1 if an iterator can be found (i.e. at least one result
     exists) or 0 if no results exist.

     Each call to iter_next will write a new value into the buffer set
     up by result with maximal length length. Only results matching
     the type specified are returned. We return length written for
     each successful iteration, and zero when we have no more items.
  */
  RESOLVER_ITER *METHOD(Resolver, get_iter, void *ctx, RDFURN uri, \
                        char *attribute);

       /* This method reads the next result from the iterator. result
	  must be an allocated and valid RDFValue object */
       int METHOD(Resolver, iter_next, RESOLVER_ITER *iter, RDFValue result);

       /* This method is similar to iter_next except the result is
	  allocated to the NULL context. Callers need to talloc_free
	  the result. This advantage of this method is that we dont
	  need to know in advance what type the value is.
       */
       RDFValue METHOD(Resolver, alloc_from_iter, RESOLVER_ITER *iter);

       /* This is a shortcut method for retrieving the encoded version
          from the iterator.
       */
       PRIVATE char *METHOD(Resolver, encoded_data_from_iter, RDFValue *rdf_value_class,
                    RESOLVER_ITER *iter);

       /* Deletes all values for this attribute from the resolver

          DEFAULT(attribute) = NULL;
        */
       void METHOD(Resolver, del, RDFURN uri, char *attribute);

       /* Expires this object and all objects it owns.

          NOTE: any outstanding objects will become invalidated. For
          example, if you expire a volume, any outstanding streams
          opened within the volume will become invalidated.
        */
       void METHOD(Resolver, expire, RDFURN uri);

     // Sets a new value for an attribute. Note that this function
     // clears any previously set values, if you want to create a list
     // of values you need to call add_value.
     int METHOD(Resolver, set_value, RDFURN uri, char *attribute, RDFValue value);

     // Adds a new value to the value list for this attribute.
     int METHOD(Resolver, add_value, RDFURN uri, char *attribute, RDFValue value);


       /** This returns a unique ID for the given URN. The ID is only
           unique within this resolver.

           DEFAULT(create) = 0
       */
       int METHOD(Resolver, get_id_by_urn, RDFURN uri, int create);

       /** This fills the URI specified by id into the uri container
       passed. Returns 1 if the ID is found, or 0 if the ID is not
       found.

       RAISES(func_return == 0, KeyError) = "URI not found for id %llu", id
       **/
       int METHOD(Resolver, get_urn_by_id, int id, RDFURN uri);

       /** This function is used to register a new RDFValue class with
           the RDF subsystem. It can then be serialised, and parsed.

           Note - the RDFValue instance must implement all the
           required methods and attributes of an RDFValue. Namely:

               dataType - the name this serialiser is known as.
               parse(serialised_form, subject)
                                      - parse itself from a serialised
                                        form
               serialise(subject)     - Return a serialised version.

               encode(subject)        - Returns a string encoding for storage in
                                        the DB
               decode(data, subject)  - Decode itself from the db.

           Note 2- this function steals a reference to the RDFValue
           object provided - this means that it must not be a
           statically allocated class template, and must be allocated
           with talloc. Use register_rdf_value_class() for static
           classes (e.g. GETCLASS(RDFURN)). Its ok to use a class
           instance in here - we will call its constructor to make new
           objects.
       **/
       void METHOD(Resolver, register_rdf_value_class, RDFValue class_ref);

#if HAVE_OPENSSL
       void METHOD(Resolver, register_security_provider, struct SecurityProvider_t *class_ref);
#endif

       /** A Utility method to create a new instance of a registered
           RDF dataType.
       */
       RDFValue METHOD(Resolver, new_rdfvalue, void *ctx, char *type);

       /* This function attempts to load the volume stored within the
          FileLikeObject URI provided. If there is a volume, the
          volume URI is set in uri, and we return true.

          We attempt to instantiate all volume drivers in turn until
          one works and then load them from the URI.
       */
       int METHOD(Resolver, load, RDFURN uri);

       /* This can be used to install a logger object. All messages
          will then be output through this object.
       */
       void METHOD(Resolver, set_logger, Logger logger);

       /** This closes and frees all memory used by the resolver.

           This is generally needed after forking as two resolvers can
           not exist in different processes.
       */
       void METHOD(Resolver, close);

       /** This is used to flush all our caches */
       void METHOD(Resolver, flush);
END_CLASS

       /* This is just a wrapper around tdb so it can easily be used
          from python etc. */
CLASS(TDB, Object)
     struct tdb_context *file;

     /** This is the constructor for a new TDB database.

         DEFAULT(mode) = 0;
     */
     TDB METHOD(TDB, Con, char *filename, int mode);
     void METHOD(TDB, store, char *key,int key_len, char *data, int len);
     TDB_DATA METHOD(TDB, fetch, char *key, int len);
END_CLASS

       /** The following are private functions */
RESOLVER_ITER *_Resolver_get_iter(Resolver self,
                                  void *ctx,
                                  TDB_DATA tdb_urn,
                                  TDB_DATA attribute);

     /* Special variants which also take the graph name.

        These should not be called externally.
      */
PRIVATE int Graph_add_value(RDFURN graph, RDFURN urn, char *attribute_str,
                            RDFValue value);

// This is a global instance of the oracle. All AFFObjects must
// communicate with the oracle rather than instantiate their own.
extern Resolver oracle;

       /** This is a handler for new types - types get registered here */
void register_type_dispatcher(char *type, AFFObject *class_ref);

       /** This following are help related function for
           introspection
       */
void print_volume_drivers();

#endif 	    /* !AFF4_RESOLVER_H_ */

