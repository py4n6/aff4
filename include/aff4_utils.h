/*
** aff4_utils.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Thu Nov 12 20:43:54 2009 mic
** Last update Thu Nov 12 20:43:54 2009 mic
*/

#ifndef   	AFF4_UTILS_H_
# define   	AFF4_UTILS_H_

#include "list.h"

#define HASH_TABLE_SIZE 256
#define CACHE_SIZE 15

// A helper to access the URN of an object.
#define URNOF(x)  ((AFFObject)x)->urn
#define STRING_URNOF(x) ((char *)URNOF(x)->value)

/** Some helper functions used to serialize int to and from URN
    attributes
*/
uint64_t parse_int(char *string);
char *from_int(uint64_t arg);
char *escape_filename(void *ctx, const char *filename, unsigned int length);
TDB_DATA unescape_filename(void *ctx, const char *filename);

TDB_DATA tdb_data_from_string(char *string);

/** A cache is an object which automatically expires data which is
    least used - that is the data which is most used is put at the end
    of the list, and when memory pressure increases we expire data
    from the front of the list.
*/
enum Cache_policy {
  CACHE_EXPIRE_FIRST,
  CACHE_EXPIRE_LEAST_USED
};

CLASS(Cache, Object)
// The key which is used to access the data
     void *key;
     int key_len;

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
     struct Cache_t *cache_head;
     enum Cache_policy policy;

     // The current number of objects managed by this cache
     int cache_size;

     // The maximum number of objects which should be managed
     int max_cache_size;

     // A hash table of the keys
     int hash_table_width;
     Cache *hash_table;

     // If this is set to 1 this Cache contains static objects and we
     // do not try to steal them:
     int static_objects;

     // These functions can be tuned to manage the hash table. The
     // default implementation assumes key is a null terminated
     // string.
     unsigned int METHOD(Cache, hash, void *key, int len);
     int METHOD(Cache, cmp, void *other, int len);

     Cache METHOD(Cache, Con, int hash_table_width, int max_cache_size);

// Return a cache object or NULL if its not there. Callers do not own
// the cache object. If they want to steal the data, they can but they
// must call talloc_free on the Cache object so it can be removed from
// the cache.
// (i.e. talloc_steal(result->data); talloc_free(result); )
     Cache METHOD(Cache, get, void *key, int len);

// A shorthand for getting the actual data itself rather than the
// Cache object itself:
     void *METHOD(Cache, get_item, char *key, int len);

// Store the key, data in a new Cache object. The key and data will be
// stolen.
     Cache METHOD(Cache, put, void *key, int len, void *data, int data_len);
END_CLASS

#endif 	    /* !AFF4_UTILS_H_ */
