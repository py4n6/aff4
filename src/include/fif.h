#ifndef __FIF_H
#define __FIF_H

#include "config.h"
#include "class.h"
#include "talloc.h"
#include "stringio.h"
#include "list.h"

uint64_t parse_int(char *string);

/** A cache is an object which automatically expires data which is
    least used - that is the data which is most used is put at the end
    of the list, and when memory pressure increases we expire data
    from the front of the list.
*/
CLASS(Cache, Object)
// The key which is used to access the data
     void *key;

     // An opaque data object and its length. The object will be
     // talloc_stealed into the cache object as we will be manging its
     // memory.
     void *data;
     int data_len;

     // Cache objects are put into two lists - the cache_list contains
     // all the cache objects currently managed by us in order of
     // least used to most used at the tail of the list. The same
     // objects are also present on one of the hash lists which hang
     // off the respective hash table. The hash_list should be shorter
     // to search linearly as it only contains objects with the same hash.
     struct list_head cache_list;
     struct list_head hash_list;

     // This is a pointer to the head of the cache
     struct Cache *cache_head;

     // The current number of objects managed by this cache
     int cache_size;

     // The maximum number of objects which should be managed
     int max_cache_size;

     // A hash table of the keys
     int hash_table_width;
     Cache *hash_table;

     // These functions can be tuned to manage the hash table. The
     // default implementation assumes key is a null terminated
     // string.
     unsigned int METHOD(Cache, hash, void *key);
     int METHOD(Cache, cmp, void *other);

     Cache METHOD(Cache, Con, int hash_table_width, int max_cache_size);

// Return a cache object or NULL if its not there. Callers do not own
// the cache object. If they want to steal the data, they can but they
// must call talloc_free on the Cache object so it can be removed from
// the cache.
// (i.e. talloc_steal(result->data); talloc_free(result); )
     Cache METHOD(Cache, get, void *key);

// A shorthand for getting the actual data itself rather than the
// Cache object itself:
     void *METHOD(Cache, get_item, char *key);

// Store the key, data in a new Cache object. The key and data will be
// stolen.
     Cache METHOD(Cache, put, void *key, void *data, int data_len);
END_CLASS

/** All AFF Objects inherit from this one. The URI must be set to
    represent the globally unique URI of this object. */
CLASS(AFFObject, Object)
     char *urn;

     // This is the type of this object
     char *type;
     
     /** Any object may be asked to be constructed from its URI */
     AFFObject METHOD(AFFObject, Con, char *uri);

     /** The is called to set properties on the object */
     void METHOD(AFFObject, set_property, char *attribute, char *value);

     /** Finally the object may be ready for use. We return the ready
	 object or NULL if something went wrong.
     */
     AFFObject METHOD(AFFObject, finish);

/** This is how an AFFObject can be created. First the oracle is asked
    to create new instance of that object:

    FileLikeObject fd = CALL(oracle, create, CLASSOF(FileLikeObject));

    Now properties can be set on the object:
    CALL(fd, set_property, "aff2:location", "file://hello.txt")

    Finally we make the object ready for use:
    CALL(fd, finish)

    and CALL(fd, write, ZSTRING_NO_NULL("foobar"))
*/

END_CLASS

// Base class for file like objects
CLASS(FileLikeObject, AFFObject)
     int64_t readptr;
     uint64_t size;
     char *name;
     
     uint64_t METHOD(FileLikeObject, seek, int64_t offset, int whence);
     int METHOD(FileLikeObject, read, char *buffer, unsigned long int length);
     int METHOD(FileLikeObject, write, char *buffer, unsigned long int length);
     uint64_t METHOD(FileLikeObject, tell);

// This closes the FileLikeObject and also frees it - it is not valid
// to use the FileLikeObject after calling this.
     void METHOD(FileLikeObject, close);
END_CLASS

// This file like object is backed by a real disk file:
CLASS(FileBackedObject, FileLikeObject)
     int fd;

     FileBackedObject METHOD(FileBackedObject, Con, char *filename, char mode);
END_CLASS

     /** The resolver is at the heart of the AFF2 specification - its
	 responsible with returning various objects from a globally
	 unique identifier (URI).
     */
CLASS(Resolver, Object)
// This is a global cache of URN and their values - we try to only
// have small URNs here and keep everything in memory.
     Cache urn;

     // This is a cache of AFFObject objects (keyed by URI) which have
     // been constructed previously. This cache is quite small and can
     // expire objects at any time. All clients of the open() method
     // get references back from this cache. They do not own any of
     // the objects and must not maintain references to them. Clients
     // may keep references to each object's urn and re-fetch the
     // object from the open method each time they want to use
     // it. This implies that clients do not need to generally do any
     // caching at all.
     Cache cache;

     Resolver METHOD(Resolver, Con);

/* This method tries to resolve the provided uri and returns an
 instance of whatever the URI refers to (As an AFFObject which is the
 common base class. You should check to see that what you get back is
 actually what you need. For example:

 FileLikeObject fd = (FileLikeObject)CALL(resolver, resolve, uri);
 if(!fd || !ISSUBCLASS(fd, FileLikeObject)) goto error;
 
 Once the resolver provides the object it is attached to the context
 ctx and removed from the cache. This ensures that the object can not
 expire from the cache while callers are holding it. For efficiency
 you must return the object to the cache as soon as possible by
 calling cache_return.
*/
      AFFObject METHOD(Resolver, open, void *ctx, char *uri);
      void METHOD(Resolver, cache_return, AFFObject obj);

/* This create a new object of the specified type. */
     AFFObject METHOD(Resolver, create, AFFObject *class_reference);

/* Returns an attribute about a particular uri if know. This may
     consult an external data source.
*/
     char *METHOD(Resolver, resolve, char *uri, char *attribute);

//Stores the uri and the value in the resolver. The value will be
//stolen, but the uri will be copied.
     void METHOD(Resolver, add, char *uri, char *attribute, char *value);

     // Exports all the properties to do with uri - user owns the buffer.
     char *METHOD(Resolver, export, char *uri);

// Deletes the attribute from the resolver
     void METHOD(Resolver, del, char *uri, char *attribute);

// This updates the value or adds it if needed
     void METHOD(Resolver, set, char *uri, char *attribute, char *value);

END_CLASS

// This is a global instance of the oracle. All AFFObjects must
// communicate with the oracle rather than instantiate their own.
extern Resolver oracle;

// This function must be called to initialise the library - we prepare
// all the classes and intantiate an oracle.
void AFF2_Init(void);

// A link simply returns the URI its pointing to
CLASS(Link, AFFObject)
     void METHOD(Link, link, Resolver resolver, char *storage_urn, 
		 char *target, char *friendly_name);
END_CLASS

/** The Image Stream represents an Image in chunks */
CLASS(Image, FileLikeObject)
// Data is divided into segments, when a segment is completed (it
// contains chunks_in_segment chunks) we dump it to the archive. These
// are stream attributes
     int chunks_in_segment;   // Default 2048
     int chunk_size;          // Default 32kb   
                              // -> Default segment size 64Mb

     // Writes get queued here until full chunks are available
     char *chunk_buffer;
     int chunk_buffer_readptr;
     int chunk_buffer_size;

     // The segment is written here until its complete and then it
     // gets flushed
     StringIO segment_buffer;
     int chunk_count;

     // Chunks are cached here. We cant use the main zip file cache
     // because the zip file holds the full segment
     Cache chunk_cache;

     int segment_count;

     // An array of indexes into the segment where chunks are stored
     int32_t *chunk_indexes;
     char *parent_urn;

END_CLASS

/** The map stream driver maps an existing stream using a
    transformation.


    We require the stream properties to specify a 'target'. This can
    either be a plain stream name or can begin with 'file://'. In the
    latter case this indicates that we should be opening an external
    file of the specified filename.

    We expect to find a component in the archive called 'map' which
    contains a mapping function. The file should be of the format:

    - lines starting with # are ignored
    
    - other lines have 2 integers seperated by white space. The first
    column is the current stream offset, while the second offset if
    the target stream offset.

    For example:
    0     1000
    1000  4000

    This means that when the current stream is accessed in the range
    0-1000 we fetch bytes 1000-2000 from the target stream, and after
    that we fetch bytes from offset 4000.

    Required properties:
    
    - target%d starts with 0 the number of target (may be specified as
      a URL). e.g. target0, target1, target2

    Optional properties:

    - file_period - number of bytes in the file offset which this map
      repreats on. (Useful for RAID)

    - image_period - number of bytes in the target image each period
      will advance by. (Useful for RAID)
*/
struct map_point {
  uint64_t file_offset;
  uint64_t image_offset;
  int target_id;
};

CLASS(MapDriver, AFFObject)
// An array of our targets
     char **target_urns;
     int number_of_targets;

     // All the points in the map:
     struct map_point *points;
     int number_of_points;
     
     // Deletes the point at the specified file offset
     void METHOD(MapDriver, del, uint64_t file_pos);

     // Adds a new point ot the file offset table
     void METHOD(MapDriver, add, uint64_t file_pos, uint64_t image_offset, 
		 FileLikeObject target);

     void METHOD(MapDriver, save_map);
END_CLASS

// A blob is a single lump of data
CLASS(Blob, AFFObject)
     char *data;
     int length;
END_CLASS

 char *resolver_get_with_default(Resolver self, char *urn, 
				 char *attribute, char *default_value);

#define URNOF(x)  ((AFFObject)x)->urn

#endif
