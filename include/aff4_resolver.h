/*
  The main AFF4 Resolver.

  The resolver is the most important part of the AFF4
  library. Virtually all access to the library is made through the
  resolver. Broadly the resolver serves to:

  1) Manage metadata storage and retrieval through the set_value(),
     add_value() methods. Lists of values can be iterated over using
     the get_iter() and iter_next() methods.

  2) Opening, locking and caching of AFF4 objects is managed via the
     open() and cache_return() methods. The resolver centrally manages
     object ownership by multiple threads. The create() method is used
     to make new instances of AFF4 objects.

  3) Manages the AFF4 engine by introducing new types for
     serialization and execution - this is done by the
     register_rdf_value_class(), register_type_dispatcher(),
     register_security_provider(), register_logger() methods.

  4) Opening and loading new AFF4 volumes which are not known to the
     system using the load() method. This also imports all metadata
     stored in the volume graphs into the resolver knowledge base.
*/

#ifndef   	AFF4_RESOLVER_H_
# define   	AFF4_RESOLVER_H_

#include "aff4_io.h"
#include "tdb.h"

struct SecurityProvider_t;

/* These are the objects which are stored in the data store */
CLASS(DataStoreObject, Object)
  char *data;
  unsigned int length;
  /* The RDFType */
  char *rdf_type;

  DataStoreObject METHOD(DataStoreObject, Con, char *data, \
                         unsigned int length, char *rdf_type);
END_CLASS

/** The abstract data store. */
CLASS(DataStore, Object)
  /* constructor.

     DEFAULT(logger) = NULL;
  */
  DataStore METHOD(DataStore, Con);

  void METHOD(DataStore, lock);
  void METHOD(DataStore, unlock);

  void METHOD(DataStore, del, char *uri, char *attribute);

  /* Set the value, removing other values. Operation is already locked. The
   * value is stolen.
   */
  void METHOD(DataStore, set, char *uri, char *attribute, \
              DataStoreObject value);

  /* Add an additional value. Operation is already locked. The value is stolen.
   */
  void METHOD(DataStore, add, char *uri, char *attribute, \
              DataStoreObject value);

  /* Get a borrowed reference to the first object. The DataStore must remain
   * locked as long as the returned object is used.
   */
  DataStoreObject METHOD(DataStore, get, char *uri, char *attribute);

  /* Iterate over all objects. The returned iterator is an opaque object. The
   * DataStore must be locked for the duration of the iteration.
   */
  Object METHOD(DataStore, iter, char *uri, char *attribute);

  /* Receive the next item with an iterator. Reference is borrowed. The
   * DataStore must be locked for the duration of the iteration.
   */
  DataStoreObject METHOD(DataStore, next, Object *iter);
END_CLASS


CLASS(MemoryDataStore, DataStore)
    int id_counter;
    Cache urn_db;
    Cache attribute_db;
    Cache data_db;

    /* This mutex protects our internal data structures */
    pthread_mutex_t mutex;
END_CLASS


DLL_PUBLIC DataStore new_MemoryDataStore(void *ctx);


/** The resolver is at the heart of the AFF4 specification - it is
    responsible for returning objects keyed by attribute from a
    globally unique identifier (URI) and managing the central
    information store.
*/
CLASS(Resolver, Object)
       /** This is where we store all our data */
       DataStore store;

       /** This is used to restore state if the RDF parser fails */
       jmp_buf env;
       char *message;

       /* Read and write caches. These have different policies. The
          read cache is just for efficiency - if an object is not in
          the cache or is used by another thread, we just create a new
          one of those.
       */
       Cache read_cache;

       /* Write cache is used for locks - it is not possible to have
          multiple write objects at the same time, and all writers are
          opened exclusively. Attempting to open an already locked
          object for writing will block until that object is returned
          to the cache.
       */
       Cache write_cache;

       /* This mutex protects our internal data structures */
       pthread_mutex_t mutex;

       /* This is used to check the type of new objects */
       RDFURN type;

       /*
         All logging takes place thorough this object which may be overridden by the user.
       */
       Logger logger;

       /*
         Main constructor for the resolver. Mode can be set to turn on
         various options, mainly for developement.

         DEFAULT(mode) = 0;
       */
       Resolver METHOD(Resolver, Con, DataStore store, int mode);

/* This method tries to resolve the provided uri and returns an
 instance of whatever the URI refers to (As an AFFObject which is the
 common base class. You should check to see that what you get back is
 actually what you need. For example:

 FileLikeObject fd = (FileLikeObject)CALL(resolver, resolve, uri);
 if(!fd || !issubclass(fd, &__FileLikeObject)) goto error;

 You must return the object to the cache as soon as possible after use
 by calling its cache_return method. The object will be locked until
 you return it with cache_return.

 DEFAULT(mode) = "r"
*/
       AFFObject METHOD(Resolver, open, RDFURN uri, char mode);

       /* This create a new object of the specified type.

          name specifies the type of object as registered in the type
          handler dispatcher. (e.g. AFF4_ZIP_VOLUME)
        */
       AFFObject METHOD(Resolver, create, RDFURN urn, char *type, char mode);

       /* This causes the resolver to take ownership of the object. The object
        * is locked to the calling thread and can be returned to the resolver at
        * any time using cache_return().
        */
       void METHOD(Resolver, manage, AFFObject obj);

       /* This method locks the object and takes ownership of it. The calling
        * thread must call cache_return() to release the object.
        */
       AFFObject METHOD(Resolver, own, RDFURN urn, char mode);

       /* All objects obtained from Resolver.open() need to be
          returned to the cache promptly using this method. NOTE - it
          is an error to be using an object after being returned to
          the cache - it might not be valid and may be gced.

          NOTE for Python bindings - do not use this function, instead
          use AFFObject.cache_return() method which ensures the
          wrapped python object is properly destroyed after this call.
       */
      void METHOD(Resolver, cache_return, AFFObject obj);


       /* This function resolves all the value in uri and attribute and sets it
          into the RDFValue object. The objects are allocated with a context of
          ctx and are chained though their list member.
       */
       RDFValue METHOD(Resolver, resolve, void *ctx, \
                       RDFURN uri, char *attribute);

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

       /* Sets a new value for an attribute. Note that this function
          clears any previously set values, if you want to create a list
          of values you need to call add_value.

          If iter is given the iterator will be filled into it. The
          iterator can subsequently be used with iter_alloc() to
          reobtain the value.

          DEFAULT(iter) = NULL;
       */
       int METHOD(Resolver, set, \
                  RDFURN uri, char *attribute, RDFValue value);

       /* Adds a new value to the value list for this attribute.

          DEFAULT(iter) = NULL;
       */
       int METHOD(Resolver, add, \
                  RDFURN uri, char *attribute, RDFValue value);

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

       void METHOD(Resolver, register_security_provider, \
                   struct SecurityProvider_t *class_ref);

       /* This can be used to install a logger object. All messages
          will then be output through this object.
       */
       void METHOD(Resolver, register_logger, Logger logger);

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

       /** A generic interface to the logger allows any code to send
           messages to the provided logger.
       */
       void METHOD(Resolver, log, int level, char *service, \
                   RDFURN subject, char *messages);

       /** This closes and frees all memory used by the resolver.

           This is generally needed after forking as two resolvers can
           not exist in different processes.
       */
       void METHOD(Resolver, close);

       /** This is used to flush all our caches */
       void METHOD(Resolver, flush);
END_CLASS

     /* This function is the main entry point into the AFF4
        library. The first thing that users need to do is obtain a
        resolver by calling AFF4_get_resolver(). Once they have a
        resolver, new instances of AFF4 objects and RDFValue objects
        can be obtained throught the open(), create(), and
        new_rdfvalue() method.
     */
Resolver AFF4_get_resolver(DataStore store, void *ctx);

/* This function can be used to register a new AFF4 type with the
   library
*/
DLL_PUBLIC void register_type_dispatcher(char *type, AFFObject *class_ref);

       /** This following are help related function for
           introspection
       */
void print_volume_drivers();

/* An exported function to free any AFF4 object. Objects are not
   really freed until their reference counts have reached 0.

   AFF4 Objects (and basically anything allocated by AFF4) can have
   their reference counts increased using aff4_incref.
*/
void aff4_free(void *ptr);
void aff4_incref(void *ptr);

     /* This function is used for debugging only. We use it to free
        miscelaneous objects which should never be freed. Never call
        this in production.
      */
void aff4_end();

void print_error_message();

#endif 	    /* !AFF4_RESOLVER_H_ */
