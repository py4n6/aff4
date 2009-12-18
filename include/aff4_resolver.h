/*
** aff4_resolver.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Thu Nov 12 20:41:24 2009 mic
** Last update Fri Dec 18 18:31:32 2009 mic
*/

#ifndef   	AFF4_RESOLVER_H_
# define   	AFF4_RESOLVER_H_

#include "aff4_io.h"

extern int AFF4_TDB_FLAGS;

// The data store is basically a singly linked list of these records:
typedef struct TDB_DATA_LIST {
  uint64_t next_offset;
  uint16_t length;

  // This type refers to the full name type as stored in types.tdb
  uint8_t encoding_type;
}__attribute__((packed)) TDB_DATA_LIST;

/** This object is returned when iterating the a result set from the
    resolver. Its basically a pointer into the resolver data store.*/
BOUND typedef struct RESOLVER_ITER {
  TDB_DATA_LIST head;
  uint64_t offset;
  // This is used to ensure we do not iterate over multiple values
  // which are the same
  Cache cache;
} RESOLVER_ITER;

/** The resolver is at the heart of the AFF4 specification - its
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
       uint32_t hashsize;
       
       /** This is used to restore state if the RDF parser fails */
       jmp_buf env;
       char *message;
       
       // Read and write caches
       Cache read_cache;
       Cache write_cache;

       // Resolvers contain the identity behind them (see below):
       struct Identity_t *identity;

       // This is used to check the type of new objects
       XSDString type;

       Resolver METHOD(Resolver, Con);
  
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
 cache_return. */
      AFFObject METHOD(Resolver, open, RDFURN uri, char mode);

       // All objected obtained from Resolver.open() need to be
       // returned to the cache promptly using this method. NOTE - it
       // is an error to be using an object after being returned to
       // the cache - it might not be valid and may be gced.
      void METHOD(Resolver, cache_return, AFFObject obj);

       /* This create a new object of the specified type.

          name specifies the type of object as registered in the type
          handler dispatcher. (e.g. AFF4_ZIP_VOLUME)
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
	  allocated. Callers need to talloc_free the result. This
	  advantage of this method is that we dont need to know in
	  advance what type the value is.
       */
     RDFValue METHOD(Resolver, iter_next_alloc, RESOLVER_ITER *iter);

     // Deletes all values for this attribute from the resolver
     void METHOD(Resolver, del, RDFURN uri, char *attribute);

     // Sets a new value for an attribute. Note that this function
     // clears any previously set values, if you want to create a list
     // of values you need to call add_value.
     void METHOD(Resolver, set_value, RDFURN uri, char *attribute, RDFValue value);

     // Adds a new value to the value list for this attribute.
     void METHOD(Resolver, add_value, RDFURN uri, char *attribute, RDFValue value);

END_CLASS

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

