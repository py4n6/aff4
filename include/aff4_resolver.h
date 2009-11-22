/*
** aff4_resolver.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Thu Nov 12 20:41:24 2009 mic
** Last update Thu Nov 12 20:41:24 2009 mic
*/

#ifndef   	AFF4_RESOLVER_H_
# define   	AFF4_RESOLVER_H_

#include "aff4_io.h"

extern int AFF4_TDB_FLAGS;
  
  /** These are the various data types which can be stored in the
      resolver. The returned (void *) pointer will be the
      corresponding type as described.
  */
  enum resolver_data_type {
    RESOLVER_DATA_UNKNOWN,   /* void */
    RESOLVER_DATA_STRING,    /* char * (null terminated) */
    RESOLVER_DATA_TDB_DATA,  /* TDB_DATA * */
    RESOLVER_DATA_UINT16,    /* uint16_t * */
    RESOLVER_DATA_UINT32,    /* uint32_t * */
    RESOLVER_DATA_UINT64,    /* uint64_t * */
    RESOLVER_DATA_OBJECT,    /* An RDFValue object */
    RESOLVER_DATA_URN,        /* char * (null terminated) */
    RESOLVER_DATA_ANY        /* A value used to indicate a wildcard -
				in this case no data will be written
				to the result but the iterator will be
				advanced. 
			     */
  };

// The data store is basically a singly linked list of these records:
typedef struct TDB_DATA_LIST {
  uint64_t next_offset;
  uint16_t length;

  // This type refers to the full name type as stored in types.tdb
  uint8_t encoding_type;
}__attribute__((packed)) TDB_DATA_LIST;

typedef struct RESOLVER_ITER {
  TDB_DATA_LIST head;
  uint64_t offset;
  enum resolver_data_type search_type;
  int end;
} RESOLVER_ITER;

     /** The resolver is at the heart of the AFF4 specification - its
	 responsible for returning objects keyed by attribute from a
	 globally unique identifier (URI).
     */

CLASS(Resolver, AFFObject)
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
 cache_return.
*/ 
      AFFObject METHOD(Resolver, open, RDFURN uri, char mode);
      void METHOD(Resolver, cache_return, AFFObject obj);

/* This create a new object of the specified type. */
      AFFObject METHOD(Resolver, create, AFFObject *class_reference);

/* Returns an attribute about a particular uri if known. This may
     consult an external data source.

     You should indicate the expected type of the attribute as
     type. If the actual type is different or not found we return NULL
     here.
*/
  void *METHOD(Resolver, resolve, void *ctx, char *uri, char *attribute, 
	       enum resolver_data_type type);

  /* This function resolves the value in uri and attribute and sets it
     into the RDFValue object which much be of the correct type. 
  */
  int METHOD(Resolver, resolve_value, char *uri, char *attribute,
	     RDFValue value);

  /** This version does not allocate any memory it just overwrite the
      preallocated memory in result (of length specified) with the
      first element returned and return the number of bytes written to
      the result.  If there are no elements we dont touch the result
      and return 0.
  */
  int METHOD(Resolver, resolve2, RDFURN uri, char *attribute, 
	     RDFValue result);

  /** This returns a null terminated list of matches. The matches will
      all be of the type specified, memory will be allocated with
      reference to the ctx specified.
  */
  void **METHOD(Resolver, resolve_list, void *ctx, char *uri, 
		char *attribute, enum resolver_data_type type);

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
  int METHOD(Resolver, get_iter, RESOLVER_ITER *iter, RDFURN uri,
	     char *attribute);
  
       /* This method reads the next result from the iterator. result
	  must be an allocated and valid RDFValue object */
  int METHOD(Resolver, iter_next, RESOLVER_ITER *iter, RDFValue result);

       /* This method is similar to iter_next except the result is
	  allocated. Callers need to talloc_free the result. This
	  advantage of this method is that we dont need to know in
	  advance what type the value is.
       */
     RDFValue METHOD(Resolver, iter_next_alloc, void *ctx, RESOLVER_ITER *iter);

//Stores the uri and the value in the resolver. The value and uri will
//be stolen.
     void METHOD(Resolver, add, RDFURN uri, char *attribute, RDFValue value);

     // Exports all the properties to do with uri - user owns the
     // buffer. context is the URN which will ultimately hold the
     // exported file. If uri is the same as context, we write the
     // statement as a relative notation.
     char *METHOD(Resolver, export_urn, char *uri, char *context);

     // Exports all the properties to do with uri - user owns the buffer.
     char *METHOD(Resolver, export_all, char *context);

     // Deletes the attribute from the resolver
     void METHOD(Resolver, del, RDFURN uri, char *attribute);

     // This updates the value or adds it if needed
     void METHOD(Resolver, set, char *uri, char *attribute, void *value,
		 char *dataType);
  
     // This is the new method that will deprecate the previous method
     void METHOD(Resolver, set_value, RDFURN uri, char *attribute, RDFValue value);

     // This is the new method that will deprecate the previous method
     void METHOD(Resolver, add_value, RDFURN uri, char *attribute, RDFValue value);

     //This returns 1 if the statement is set
     int METHOD(Resolver, is_set, RDFURN uri, char *attribute, RDFValue value);

     // Parses the properties file
     void METHOD(Resolver, parse, char *context, char *text, int len);

  /* The following APIs are used for synchronization. 
     A thread begins a transaction - this locks the resolver from
     access from all other threads.

     One thread
     creates a lock on an object (Using its URN) by calling:
     
     oracle->lock(urn, name)

     Other threads can then attempt to also get a lock on the URN but
     will be blocked until the original thread calls:
     
     oracle->unlock(urn, name) 

     Note that locks have names and so many locks can be set on the
     same URN.
  */
  int METHOD(Resolver, lock, RDFURN urn, char name);
  int METHOD(Resolver, unlock, RDFURN urn, char name);

END_CLASS

// This is a global instance of the oracle. All AFFObjects must
// communicate with the oracle rather than instantiate their own.
extern Resolver oracle;

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/pem.h>

/** This object represents an identify - like a person for example. 

An identity is someone who makes statements about other AFFObjects in
the universe.
*/
CLASS(Identity, AFFObject)
       Resolver info;
       
       EVP_PKEY *priv_key;
       EVP_PKEY *pub_key;
       X509 *x509;

       Identity METHOD(Identity, Con, char *cert, char *priv_key, char mode);
       void METHOD(Identity, store, char *volume_urn);
  /** This method asks the identity to verify its statements. This
       essentially populates our Resolver with statements which can be
       verified from our statements. Our Resolver can then be
       compared to the oracle to see which objects do not match.
  */
       void METHOD(Identity, verify, int (*cb)(uint64_t progress, char *urn));
END_CLASS

       /** This is a handler for new types - types get registered here */
void register_type_dispatcher(char *type, AFFObject *class_ref);

#endif 	    /* !AFF4_RESOLVER_H_ */
