#include <zlib.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/aes.h>
#include <setjmp.h>

#include <assert.h>

#define BUFF_SIZE 40960

#ifdef WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <stdio.h>
#define MSG_NOSIGNAL 0
typedef unsigned long int in_addr_t;
#else
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <uuid/uuid.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <error.h>
#include <errno.h>
#include <pthread.h>
#include <setjmp.h>
#include <fcntl.h>

#define O_BINARY 0
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif

// Some required definitions from class.h if not already used
#ifndef ___CLASS_H
typedef struct Object_t *Object;

struct Object_t {
	Object __class__;
	Object __super__;
	char *__name__;
	char *__doc__;
	int __size;
};

extern struct Object_t __Object;

/** Find out if obj is an instance of cls or a derived class.

    Use like this:

    if(issubclass(obj, (Object)&__FileLikeObject)) {
       ...
    };

    You can also do this in a faster way if you already know the class
    hierarchy (but it could break if the hierarchy changes):
    {
     Object cls = ((Object)obj)->__class__;

     if(cls == (Object)&__Image || \
        cls == (Object)&__FileLikeObject || \
        cls == (Object)&__AFFObject || ....) {
     ...
     };
    };
 */
int issubclass(Object obj, Object cls);

extern void unimplemented(Object self);
#endif

// Some required definitions from raptor if not already defined
#ifndef RAPTOR_H
typedef enum {
	RAPTOR_IDENTIFIER_TYPE_UNKNOWN,
	RAPTOR_IDENTIFIER_TYPE_RESOURCE,
	RAPTOR_IDENTIFIER_TYPE_ANONYMOUS,
	RAPTOR_IDENTIFIER_TYPE_PREDICATE,
	RAPTOR_IDENTIFIER_TYPE_ORDINAL,
	RAPTOR_IDENTIFIER_TYPE_LITERAL,
	RAPTOR_IDENTIFIER_TYPE_XML_LITERAL
} raptor_identifier_type;

typedef void *raptor_uri;

typedef void *raptor_serializer;

typedef void *raptor_iostream;

typedef void *raptor_statement;

typedef void *raptor_locator;
#endif

#ifndef __TDB_H__
typedef struct TDB_DATA {
	unsigned char *dptr;
	size_t dsize;
} TDB_DATA;
#endif

typedef void *Queue;

typedef void *StringIO;

// A helper to access the URN of an object.
#define URNOF(x)  ((AFFObject)x)->urn
#define STRING_URNOF(x) ((char *)URNOF(x)->value)
#ifndef __LIST_H
#define __LIST_H

/* This file is from Linux Kernel (include/linux/list.h) 
 * and modified by simply removing hardware prefetching of list items. 
 * Here by copyright, credits attributed to wherever they belong.
 * Kulesh Shanmugasundaram (kulesh [squiggly] isis.poly.edu)
 */

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

/*
 * Insert a new entry between two known consecutive entries. 
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_add(struct list_head *_new,
			      struct list_head *prev, struct list_head *next)
{
	next->prev = _new;
	_new->next = next;
	_new->prev = prev;
	prev->next = _new;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *_new, struct list_head *head)
{
	__list_add(_new, head, head->next);
}

/**
 * list_add_tail - add a _new entry
 * @_new: _new entry to be added
 * @head: list head to add it before
 *
 * Insert a _new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *_new, struct list_head *head)
{
	__list_add(_new, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty on entry does not return true after this, the entry is in an undefined state.
 */
static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = NULL;
	entry->prev = NULL;
}

/**
 * list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

/**
 * list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add(list, head);
}

/**
 * list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void list_move_tail(struct list_head *list,
				  struct list_head *head)
{
	__list_del(list->prev, list->next);
	list_add_tail(list, head);
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(struct list_head *head)
{
	return head->next == head;
}

static inline void __list_splice(struct list_head *list, struct list_head *head)
{
	struct list_head *first = list->next;

	struct list_head *last = list->prev;

	struct list_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

/**
 * list_splice - join two lists
 * @list: the _new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head);
}

/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the _ne list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void list_splice_init(struct list_head *list,
				    struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head);
		INIT_LIST_HEAD(list);
	}
}

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); \
        	pos = pos->next)

/**
 * list_for_each_prev	-	iterate over a list backwards
 * @pos:	the &struct list_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); \
        	pos = pos->prev)

/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop counter.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_for_each_entry_prev(pos, head, member)				\
	for (pos = list_entry((head)->prev, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = list_entry(pos->member.prev, typeof(*pos), member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

#define list_for_each_entry_safe_prev(pos, n, head, member)			\
	for (pos = list_entry((head)->prev, typeof(*pos), member),	\
		n = list_entry(pos->member.prev, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.prev, typeof(*n), member))

/**
 * list_count - count number of entries in list
 * @head: head of the list to count
 */
static inline int list_count(struct list_head *head)
{
	int count = 0;

	struct list_head *tmp;

	list_for_each(tmp, head) {
		count++;
	}
	return count;
}

/** Given a list head, returns the first entry and assigns to i */
#define list_next(first, head, member)			\
  do { first=list_entry((head)->next, typeof(*first), member); } while(0)

#define list_prev(first, head, member)			\
  do { first=list_entry((head)->prev, typeof(*first), member); } while(0)

#endif

/** This file defines constants used in AFF4. These constants are
    required to ensure that AFF4 files are compatible between
    implementations. We therefore keep this file seperate and refer
    to the constants defined here everywhere else.
*/

// This is the URI namespace for the AFF4 scheme
#define NAMESPACE "aff4://"
#define PREDICATE_NAMESPACE "http://afflib.org/2009/aff4#"
#define RDF_NAMESPACE "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#define XSD_NAMESPACE "http://www.w3.org/2001/XMLSchema#"

// Standard RDFValue dataTypes:
#define DATATYPE_XSD_STRING    ""
#define DATATYPE_XSD_INTEGER   XSD_NAMESPACE "integer"
#define DATATYPE_RDF_URN       RDF_NAMESPACE "urn"
#define DATATYPE_XSD_DATETIME  XSD_NAMESPACE "dateTime"

// RDFValues we use
#define AFF4_MAP_TEXT PREDICATE_NAMESPACE "map_text"
#define AFF4_MAP_BINARY PREDICATE_NAMESPACE "map_binary"
#define AFF4_MAP_INLINE PREDICATE_NAMESPACE "map_inline"
#define AFF4_MAP_TARGET_COUNT PREDICATE_NAMESPACE "map_target_count"
#define AFF4_INTEGER_ARRAY_BINARY PREDICATE_NAMESPACE "integer_array_binary"
#define AFF4_INTEGER_ARRAY_INLINE PREDICATE_NAMESPACE "integer_array_inline"

// Various namespaces
#define VOLATILE_NS "aff4volatile:"
#define CONFIGURATION_NS VOLATILE_NS "config:"
#define FQN NAMESPACE ""

// This object holds parameters pertinent to the running instance
#define GLOBAL VOLATILE_NS "global"

// Configuration parameters
#define CONFIG_THREADS  CONFIGURATION_NS  "threads"
#define CONFIG_VERBOSE  CONFIGURATION_NS  "verbosity"
#define CONFIG_AUTOLOAD  CONFIGURATION_NS "autoload"
#define CONFIG_PAD       CONFIGURATION_NS "pad"

/** These are standard aff4 attributes */
#define AFF4_STORED     PREDICATE_NAMESPACE "stored"
#define AFF4_TYPE       RDF_NAMESPACE "type"
#define AFF4_INTERFACE  PREDICATE_NAMESPACE "interface"
#define AFF4_SIZE       PREDICATE_NAMESPACE "size"
#define AFF4_SHA        PREDICATE_NAMESPACE "sha256"
#define AFF4_TIMESTAMP  PREDICATE_NAMESPACE "timestamp"
#define AFF4_MAP_DATA   PREDICATE_NAMESPACE "map"

/** These are volatile versions of the above - they are mostly used
    for segments which pass these using the Zip file format itself. 
*/
#define AFF4_VOLATILE_SIZE VOLATILE_NS "size"
#define AFF4_VOLATILE_TIMESTAMP VOLATILE_NS "timestamp"
#define AFF4_VOLATILE_STORED VOLATILE_NS "stored"
#define AFF4_VOLATILE_TYPE VOLATILE_NS "type"

// Supported interfaces
#define AFF4_STREAM     PREDICATE_NAMESPACE "stream"
#define AFF4_VOLUME     PREDICATE_NAMESPACE "volume"

/** ZipFile attributes */
#define AFF4_VOLATILE_HEADER_OFFSET VOLATILE_NS "relative_offset_local_header"
#define AFF4_VOLATILE_COMPRESSED_SIZE VOLATILE_NS "compress_size"
#define AFF4_VOLATILE_CRC VOLATILE_NS "crc32"
#define AFF4_VOLATILE_COMPRESSION VOLATILE_NS "compression"
#define AFF4_VOLATILE_FILE_OFFSET VOLATILE_NS "file_offset"
#define AFF4_VOLATILE_DIRTY VOLATILE_NS "dirty"
#define AFF4_VOLATILE_CONTAINS   VOLATILE_NS "contains"
#define AFF4_CONTAINS   VOLATILE_NS "contains"

//Its useful for debugging to enable this so the RDF file also shows
//the contains property for a volume. It is not needed normally since
//its infered from the zip file itself.
//#define AFF4_VOLATILE_CONTAINS   PREDICATE_NAMESPACE "contains"

// Volume attributes
#define AFF4_IDENTITY_STORED PREDICATE_NAMESPACE "identity"

 /*
    A hint that this stream should be automatically loaded as a
    volume 
  */
#define AFF4_AUTOLOAD PREDICATE_NAMESPACE "autoload"

/** Image attributes */
#define AFF4_CHUNK_SIZE PREDICATE_NAMESPACE "chunk_size"
#define AFF4_COMPRESSION PREDICATE_NAMESPACE "compression"
#define AFF4_CHUNKS_IN_SEGMENT PREDICATE_NAMESPACE "chunks_in_segment"
#define AFF4_DIRECTORY_OFFSET VOLATILE_NS "directory_offset"

/** Link, encryption attributes */
#define AFF4_TARGET PREDICATE_NAMESPACE "target"

/** Map attributes */
#define AFF4_IMAGE_PERIOD PREDICATE_NAMESPACE "image_period"
#define AFF4_TARGET_PERIOD PREDICATE_NAMESPACE "target_period"

/* Identity attributes */
#define AFF4_STATEMENT PREDICATE_NAMESPACE "statement"
#define AFF4_CERT      PREDICATE_NAMESPACE "x509"
#define AFF4_PRIV_KEY  VOLATILE_NS "priv_key"
#define AFF4_COMMON_NAME PREDICATE_NAMESPACE "common_name"
#define AFF4_IDENTITY_PREFIX  FQN  "identity"
#define AFF4_HASH_TYPE        FQN  "hash_type"

// A property indicating this object should be highlighted
#define AFF4_HIGHLIGHT        PREDICATE_NAMESPACE   "highlight"

// Information segments are named starting with this (and ending with
// the RDF encoding):
#define AFF4_INFORMATION     "information."

// Encrypted stream attributes
// Thats the passphrase that will be used to encrypt the session key
#define AFF4_VOLATILE_PASSPHRASE VOLATILE_NS "passphrase"
#define AFF4_ENV_PASSPHRASE "AFF4_PASSPHRASE"

// This is the master key for encryption (Never written down)
#define AFF4_VOLATILE_KEY               VOLATILE_NS "key"
#define AFF4_CRYPTO_NAMESPACE           NAMESPACE "crypto:"

/* The intermediate key is obtained from pbkdf2() of the
   passphrase and salt. Iteration count is the fortification.*/
#define AFF4_CRYPTO_FORTIFICATION_COUNT AFF4_CRYPTO_NAMESPACE "fortification"
#define AFF4_CRYPTO_IV       AFF4_CRYPTO_NAMESPACE "iv"
#define AFF4_CRYPTO_RSA      AFF4_CRYPTO_NAMESPACE "rsa"

// This is the image master key encrypted using the intermediate key
#define AFF4_CRYPTO_PASSPHRASE_KEY      AFF4_CRYPTO_NAMESPACE "passphrase_key"
#define AFF4_CRYPTO_ALGORITHM           AFF4_CRYPTO_NAMESPACE "algorithm"
#define AFF4_CRYPTO_BLOCKSIZE           AFF4_CRYPTO_NAMESPACE "blocksize"

/* The nonce is the salt encrypted using the image master key. Its
   used to check the master key is correct: */
#define AFF4_CRYPTO_NONCE               AFF4_CRYPTO_NAMESPACE "nonce"

// Supported algorithms
#define AFF4_CRYPTO_ALGORITHM_AES_SHA254 "AES256/SHA256"

/** These are standard aff4 types. */
#define AFF4_GRAPH            PREDICATE_NAMESPACE "graph"

/** Volume types **/
#define AFF4_ZIP_VOLUME       PREDICATE_NAMESPACE "zip_volume"
#define AFF4_DIRECTORY_VOLUME PREDICATE_NAMESPACE "directory"
#define AFF4_LIBAFF_VOLUME    PREDICATE_NAMESPACE "aff1_volume"
#define AFF4_EWF_VOLUME       PREDICATE_NAMESPACE "ewf_volume"

/** Stream types */
#define AFF4_SEGMENT          PREDICATE_NAMESPACE "segment"
#define AFF4_LINK             PREDICATE_NAMESPACE "link"
#define AFF4_IMAGE            PREDICATE_NAMESPACE "image"
#define AFF4_MAP              PREDICATE_NAMESPACE "map"
#define AFF4_ENCRYTED         PREDICATE_NAMESPACE "encrypted"
#define AFF4_LIBAFF_STREAM    PREDICATE_NAMESPACE "aff1_stream"
#define AFF4_ERROR_STREAM     PREDICATE_NAMESPACE "error"
#define AFF4_FILE             "file"
#define AFF4_EWF_STREAM       PREDICATE_NAMESPACE "ewf_stream"

#define AFF4_INDEX            PREDICATE_NAMESPACE "index"

/** Miscellaneous objects */
#define AFF4_IDENTITY         "identity"

/* The following URNs are special and should be known by the
   implementation: */

/* This URN refers to NULL data in Sparse maps (unread data not the
   same as zero) */
#define AFF4_SPECIAL_URN_NULL FQN "null"

 /*
    This is used to represent long runs of zero 
  */
#define AFF4_SPECIAL_URN_ZERO FQN "zero"

#define AFF4_NAVIGATION       PREDICATE_NAMESPACE "nav"

/* A well known URL which is used to anchor navigation directives. */
#define AFF4_NAVIGATION_ROOT  FQN "navigation/root"
#define AFF4_NAVIGATION_CHILD  PREDICATE_NAMESPACE "NavChild"

/* This links the navigation node to a real AFF4 node */
#define AFF4_NAVIGATION_LINK   PREDICATE_NAMESPACE "NavLink"

/** Cryptography related names */
#define AFF4_CIPHER PREDICATE_NAMESPACE "cipher"

/** A cipher that uses a password */
#define AFF4_AES256_PASSWORD PREDICATE_NAMESPACE "aes256-password"

/* A cipher that uses x509 certs */
#define AFF4_AES256_X509     PREDICATE_NAMESPACE "aes256-x509"

/** The following are endoresed vocabulary for storing common forensic
    properties.
*/
#define AFF4_UNIX_PERMS PREDICATE_NAMESPACE "unix_perms"
#define AFF4_MTIME      PREDICATE_NAMESPACE "mtime"
#define AFF4_ATIME      PREDICATE_NAMESPACE "atime"
#define AFF4_CTIME      PREDICATE_NAMESPACE "ctime"
#define AFF4_FILE_TYPE  PREDICATE_NAMESPACE "type/magic"

/** These modes can be used in the resolver */
#define RESOLVER_MODE_PERSISTANT 0
#define RESOLVER_MODE_NONPERSISTANT 1
#define RESOLVER_MODE_DEBUG_MEMORY 2

/** Objects can be marked as dirty in a number of cases: */
#define DIRTY_STATE_UNKNOWN 0

/** We need to close this volume - it is inconsistent */
#define DIRTY_STATE_NEED_TO_CLOSE 1

/** The volume is not dirty, and there is no need to reparse its
    information. This indicates that the volume and the resolver are
    in sync and allows us to load it quickly. */
#define DIRTY_STATE_ALREADY_LOADED 2

/* This is the largest size of the inline integer array - bigger
   arrays get their own segments.
*/
#define MAX_SIZE_OF_INLINE_ARRAY 100

/** The following belong to the error and reporting subsystem */

/* Levels */
#define AFF4_LOG_MESSAGE 1
#define AFF4_LOG_WARNING 2
#define AFF4_LOG_NONFATAL_ERROR 3
#define AFF4_LOG_FATAL_ERROR 4

/* Services */
#define AFF4_SERVICE_GENERIC  "AFF4"
#define AFF4_SERVICE_RESOLVER "Resolver"
#define AFF4_SERVICE_IMAGE_STREAM   "Image Stream"
#define AFF4_SERVICE_ENCRYPTED_STREAM   "Encrypted Stream"
#define AFF4_SERVICE_CRYPTO_SUBSYS   "Crypto Subsystem"
#define AFF4_SERVICE_MAP_STREAM   "Map Stream"
#define AFF4_SERVICE_ZIP_VOLUME   "Zip Volume"
#define AFF4_SERVICE_RDF_SUBSYSYEM "RDF Subsystem"

/*
** aff4_utils.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Thu Nov 12 20:43:54 2009 mic
** Last update Wed Jan 27 11:02:12 2010 mic
*/

/** A cache is an object which automatically expires data which is
    least used - that is the data which is most used is put at the end
    of the list, and when memory pressure increases we expire data
    from the front of the list.
*/
enum Cache_policy {
	CACHE_EXPIRE_FIRST,
	CACHE_EXPIRE_LEAST_USED
};

/** The Cache is an object which manages a cache of Object instances
    indexed by a key.

    Each Object can either be in the Cache (in which case it is owned
    by this cache object and will not be freed) or out of the cache
    (in which case it is owned by NULL, and can be freed. When the
    cache gets too full it will start freeing objects it owns.

    Objects are cached with a key reference so this is like a python
    dict or a hash table, except that we can have several objects
    indexed with the same key. If you want to iterate over all the
    objects you need to use the iterator interface.

    NOTE: The cache must be locked if you are using it from multiple
    threads. You can use the Cache->lock mutex or some other mutex
    instead.

    NOTE: After putting the Object in the cache you do not own it -
    and you must not use it (because it might be freed at any time).
*/
typedef struct Cache_t *Cache;

Cache alloc_Cache();		/* Allocates object memory */

int Cache_init(Object self);	/* Class initializer */

extern struct Cache_t __Cache;	/* Public class template */

struct Cache_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	Cache __class__;	/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */

/* The key which is used to access the data */
	char *key;
	int key_len;
	/*
	   An opaque data object and its length. The object will be
	   talloc_stealed into the cache object as we will be manging its
	   memory.
	 */
	Object data;
	/*
	   Cache objects are put into two lists - the cache_list contains
	   all the cache objects currently managed by us in order of
	   least used to most used at the tail of the list. The same
	   objects are also present on one of the hash lists which hang
	   off the respective hash table. The hash_list should be shorter
	   to search linearly as it only contains objects with the same
	   hash.
	 */
	struct list_head cache_list;
	struct list_head hash_list;
	/*
	   This is a pointer to the head of the cache 
	 */
	struct Cache_t *cache_head;
	enum Cache_policy policy;
	/*
	   The current number of objects managed by this cache 
	 */
	int cache_size;
	/*
	   The maximum number of objects which should be managed 
	 */
	int max_cache_size;
	/*
	   A hash table of the keys 
	 */
	int hash_table_width;
	Cache *hash_table;
	/*
	   These functions can be tuned to manage the hash table. The
	   default implementation assumes key is a null terminated
	   string.
	 */
	unsigned int (*hash) (Cache self, char *key, int len);
	int (*cmp) (Cache self, char *other, int len);

     /** hash_table_width is the width of the hash table.
         if max_cache_size is 0, we do not expire items.

         DEFAULT(hash_table_width) = 100;
         DEFAULT(max_cache_size) = 0;
     */
	 Cache(*Con) (Cache self, int hash_table_width, int max_cache_size);
	/*
	   Return a cache object or NULL if its not there. The
	   object is removed from the cache.
	 */
	 Object(*get) (Cache self, char *key, int len);
	/*
	   Returns a reference to the object. The object is still owned
	   by the cache. Note that this should only be used in caches which
	   do not expire objects or if the cache is locked, otherwise the
	   borrowed reference may disappear unexpectadly. References may be
	   freed when other items are added.
	 */
	 Object(*borrow) (Cache self, char *key, int len);
	/*
	   Store the key, data in a new Cache object. The key and data will be
	   stolen.
	 */
	 Cache(*put) (Cache self, char *key, int len, Object data);
	/*
	   Returns true if the object is in cache 
	 */
	int (*present) (Cache self, char *key, int len);
	/*
	   This returns an opaque reference to a cache iterator. Note:
	   The cache must be locked the entire time between receiving the
	   iterator and getting all the objects.
	 */
	 Object(*iter) (Cache self, char *key, int len);
	 Object(*next) (Cache self, Object * iter);
	int (*print_cache) (Cache self);
};

struct RDFURN_t;

     /** A logger may be registered with the Resolver. Any objects
         obtained from the Resolver will then use the logger to send
         messages to the user.
     */
typedef struct Logger_t *Logger;

Logger alloc_Logger();		/* Allocates object memory */

int Logger_init(Object self);	/* Class initializer */

extern struct Logger_t __Logger;	/* Public class template */

struct Logger_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	Logger __class__;	/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */
	 Logger(*Con) (Logger self);
	/*
	   The message sent to the logger is sent from a service (The
	   source of the message), at a particular level (how critical it
	   is). It talks about a subject (usually the URN of the subject),
	   and a message about it.
	 */
	void (*message) (Logger self, int level, char *service,
			 struct RDFURN_t * subject, char *message);
};

;
struct RDFURN_t;

/**** A class used to parse URNs */
typedef struct URLParse_t *URLParse;

URLParse alloc_URLParse();	/* Allocates object memory */

int URLParse_init(Object self);	/* Class initializer */

extern struct URLParse_t __URLParse;	/* Public class template */

struct URLParse_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	URLParse __class__;	/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */
	char *scheme;
	char *netloc;
	char *query;
	char *fragment;
	void *ctx;
	 URLParse(*Con) (URLParse self, char *url);
	/*
	   Parses the url given and sets internal attributes. 
	 */
	int (*parse) (URLParse self, char *url);
	/*
	   Returns the internal attributes joined together into a valid
	   URL.
	 */
	char *(*string) (URLParse self, void *ctx);
};

/***** The RDF resolver stores objects inherited from this one.

       You can define other objects and register them using the
       register_rdf_value_class() function. You will need to extend
       this base class and initialise it with a unique dataType URN.

       RDFValue is the base class for all other values.
******/
typedef struct RDFValue_t *RDFValue;

RDFValue alloc_RDFValue();	/* Allocates object memory */

int RDFValue_init(Object self);	/* Class initializer */

extern struct RDFValue_t __RDFValue;	/* Public class template */

struct RDFValue_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	RDFValue __class__;	/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */
	char *dataType;
	int id;
	raptor_identifier_type raptor_type;
	/*
	   These flags will be stored in the TDB_DATA_LIST and specify
	   our behaviour - used for optimizations.
	 */
	uint8_t flags;
	/*
	   This is only required for special handling - Leave as NULL to
	   be the same as dataType above
	 */
	raptor_uri *raptor_literal_datatype;
	 RDFValue(*Con) (RDFValue self);
	/*
	   This method is called to parse a serialised form into this
	   instance. Return 1 if parsing is successful, 0 if error
	   occured.

	   DEFAULT(subject) = NULL;
	 */
	int (*parse) (RDFValue self, char *serialised_form,
		      struct RDFURN_t * subject);
	/*
	   This method is called to serialise this object into the
	   TDB_DATA struct for storage in the TDB data store. The new
	   memory will be allocated with this object's context and must
	   be freed by the caller.
	 */
	TDB_DATA *(*encode) (RDFValue self, struct RDFURN_t * subject);
	/*
	   This method is used to decode this object from the
	   data_store. The fd is seeked to the start of this record.
	 */
	int (*decode) (RDFValue self, char *data, int length,
		       struct RDFURN_t * subject);

      /** This method will serialise the value into a null terminated
	  string for export into RDF. The returned string will be
	  allocated to the NULL context and should be unlinked by the caller.

          DEFAULT(subject) = NULL;
      */
	char *(*serialise) (RDFValue self, struct RDFURN_t * subject);

      /** Makes a copy of this value - the copy should be made as
          efficiently as possible (It might just take a reference to
          memory instead of copying it).
      */
	 RDFValue(*clone) (RDFValue self, void *ctx);
};

      /** The following is a direction for the autogenerator to create
          a proxied class for providing a python class as an
          implementation of RDFValue.
      */

      /** An integer encoded according the XML RFC. */
typedef struct XSDInteger_t *XSDInteger;

XSDInteger alloc_XSDInteger();	/* Allocates object memory */

int XSDInteger_init(Object self);	/* Class initializer */

extern struct XSDInteger_t __XSDInteger;	/* Public class template */

struct XSDInteger_t {
	struct RDFValue_t super;	/* Superclass Fields we inherit */
	XSDInteger __class__;	/* Pointer to our own class */
	RDFValue __super__;	/* Pointer to our superclass */
	int64_t value;
	char *serialised;
	void (*set) (XSDInteger self, uint64_t value);
	 uint64_t(*get) (XSDInteger self);
};

     /** A literal string */
typedef struct XSDString_t *XSDString;

XSDString alloc_XSDString();	/* Allocates object memory */

int XSDString_init(Object self);	/* Class initializer */

extern struct XSDString_t __XSDString;	/* Public class template */

struct XSDString_t {
	struct RDFValue_t super;	/* Superclass Fields we inherit */
	XSDString __class__;	/* Pointer to our own class */
	RDFValue __super__;	/* Pointer to our superclass */
	char *value;
	int length;
	void (*set) (XSDString self, char *string, int length);
	char *(*get) (XSDString self);
};

     /** Dates serialised according the XML standard */
typedef struct XSDDatetime_t *XSDDatetime;

XSDDatetime alloc_XSDDatetime();	/* Allocates object memory */

int XSDDatetime_init(Object self);	/* Class initializer */

extern struct XSDDatetime_t __XSDDatetime;	/* Public class template */

struct XSDDatetime_t {
	struct RDFValue_t super;	/* Superclass Fields we inherit */
	XSDDatetime __class__;	/* Pointer to our own class */
	RDFValue __super__;	/* Pointer to our superclass */
	struct timeval value;
	int gm_offset;
	char *serialised;
	void (*set) (XSDDatetime self, struct timeval time);
};

     /** A URN for use in the rest of the library */
typedef struct RDFURN_t *RDFURN;

RDFURN alloc_RDFURN();		/* Allocates object memory */

int RDFURN_init(Object self);	/* Class initializer */

extern struct RDFURN_t __RDFURN;	/* Public class template */

struct RDFURN_t {
	struct RDFValue_t super;	/* Superclass Fields we inherit */
	RDFURN __class__;	/* Pointer to our own class */
	RDFValue __super__;	/* Pointer to our superclass */

/** This is a normalised string version of the currently set URL. Note
    that we obtain this by parsing the URL and then serialising it -
    so this string is a normalized URL.
 */
	char *value;
	/*
	   This parser maintains our internal state. We use it to parse
	   elements.
	 */
	URLParse parser;
	void (*set) (RDFURN self, char *urn);
	/*
	   Make a new RDFURN as a copy of this one 
	 */
	 RDFURN(*copy) (RDFURN self, void *ctx);
	/*
	   Add a relative stem to the current value. If urn is a fully
	   qualified URN, we replace the current value with it.
	 */
	void (*add) (RDFURN self, char *urn);

     /** This adds the binary string in filename into the end of the
     URL query string, escaping invalid characters.
     */
	void (*add_query) (RDFURN self, unsigned char *query, unsigned int len);
	/*
	   This method returns the relative name 
	 */
	 TDB_DATA(*relative_name) (RDFURN self, RDFURN volume);
};

     /** An integer array stores an array of integers
         efficiently. This variant stores it in a binary file.
     */
typedef struct IntegerArrayBinary_t *IntegerArrayBinary;

IntegerArrayBinary alloc_IntegerArrayBinary();	/* Allocates object memory */

int IntegerArrayBinary_init(Object self);	/* Class initializer */

extern struct IntegerArrayBinary_t __IntegerArrayBinary;	/* Public class template */

struct IntegerArrayBinary_t {
	struct RDFValue_t super;	/* Superclass Fields we inherit */
	IntegerArrayBinary __class__;	/* Pointer to our own class */
	RDFValue __super__;	/* Pointer to our superclass */
	uint32_t *array;
	/*
	   Current pointer for adding members 
	 */
	int current;
	/*
	   The total number of indexes in this array 
	 */
	int size;
	/*
	   The available allocated memory (we allocate chunks of BUFF_SIZE) 
	 */
	int alloc_size;
	/*
	   The urn of the array is derived from the subject by appending
	   this extension. 
	 */
	char *extension;

     /** Add a new integer to the array */
	void (*add) (IntegerArrayBinary self, unsigned int offset);
};

     /** This is used when the array is fairly small and it can fit
         inline */
typedef struct IntegerArrayInline_t *IntegerArrayInline;

IntegerArrayInline alloc_IntegerArrayInline();	/* Allocates object memory */

int IntegerArrayInline_init(Object self);	/* Class initializer */

extern struct IntegerArrayInline_t __IntegerArrayInline;	/* Public class template */

struct IntegerArrayInline_t {
	struct IntegerArrayBinary_t super;	/* Superclass Fields we inherit */
	IntegerArrayInline __class__;	/* Pointer to our own class */
	IntegerArrayBinary __super__;	/* Pointer to our superclass */
};

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

RDFURN new_RDFURN(void *ctx);

XSDInteger new_XSDInteger(void *ctx);

XSDString new_XSDString(void *ctx);

XSDDatetime new_XSDDateTime(void *ctx);

// Generic constructor for an RDFValue
RDFValue new_rdfvalue(void *ctx, char *type);

/*
** aff4_io.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Thu Nov 12 20:38:45 2009 mic
** Last update Tue Feb  2 16:24:59 2010 mic
*/
struct RDFValue;

/** All AFF Objects inherit from this one. The URI must be set to
    represent the globally unique URI of this object. */
typedef struct AFFObject_t *AFFObject;

AFFObject alloc_AFFObject();	/* Allocates object memory */

int AFFObject_init(Object self);	/* Class initializer */

extern struct AFFObject_t __AFFObject;	/* Public class template */

struct AFFObject_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	AFFObject __class__;	/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */
	RDFURN urn;
	// An object may be owned by a single thread at a time
	pthread_mutex_t mutex;
	pthread_t thread_id;
	// Is this object a reader or a writer?
	char mode;
	// This is the rdf:type property of this object
	char *dataType;
	// This is set to 1 when the object is complete (i.e. we called
	// finish on it). Methods should check that the object is
	// completed properly before using it.
	int complete;

     /** Any object may be asked to be constructed from its URI.

         DEFAULT(urn) = NULL;
         DEFAULT(mode) = "w";
      */
	 AFFObject(*Con) (AFFObject self, RDFURN urn, char mode);

     /** This is called to set properties on the object */
	void (*set) (AFFObject self, char *attribute, RDFValue value);
	void (*add) (AFFObject self, char *attribute, RDFValue value);

     /** Finally the object may be ready for use. We return the ready
	 object or NULL if something went wrong.
     */
	 AFFObject(*finish) (AFFObject self);

     /** This method is used to return this object to the primary
     resolver cache. The object should not be used after calling this
     as the caller no longer owns it. As far as the caller is
     concerned this is a desctructor and if you need the object again,
     you need to call Resolver.open() to reobtain this.

     In practice this method synchronises the object attributes so
     that a subsequent call to open with a cache miss will be able to
     reconstruct this object exactly as it is now. Once these
     attributes are set, this function calls Resolver.cache_return to
     place the object back in the cache.

     Sometimes this is impossible to do accurately, in which case the
     function can simply choose to free the object and not return it
     to the cache.
     */
	void (*cache_return) (AFFObject self);
	/*
	   When the object is closed it will write itself to its volume.
	   This frees the object - do no use it after calling close.
	 */
	int (*close) (AFFObject self);
	/*
	   This method is used to delete the object from the resolver.
	   It should also call the delete method for all the objects
	   contained within this one or that can be inferred to be
	   incorrect.

	   This method is primarily used to ensure that when
	   inconsistant information is found about the world, the
	   resolver information is invalidated.
	 */
	void (*delete) (RDFURN urn);

/** This is how an AFFObject can be created. First the oracle is asked
    to create new instance of that object:

    FileLikeObject fd = CALL(oracle, create, CLASSOF(FileLikeObject));

    Now properties can be set on the object:
    CALL(fd, set_property, "aff2:location", "file://hello.txt")

    Finally we make the object ready for use:
    CALL(fd, finish)

    and CALL(fd, write, ZSTRING_NO_NULL("foobar"))
*/
};

;
// Base class for file like objects
typedef struct FileLikeObject_t *FileLikeObject;

FileLikeObject alloc_FileLikeObject();	/* Allocates object memory */

int FileLikeObject_init(Object self);	/* Class initializer */

extern struct FileLikeObject_t __FileLikeObject;	/* Public class template */

struct FileLikeObject_t {
	struct AFFObject_t super;	/* Superclass Fields we inherit */
	FileLikeObject __class__;	/* Pointer to our own class */
	AFFObject __super__;	/* Pointer to our superclass */
	int64_t readptr;
	XSDInteger size;
	char *data;

     /** Seek the file like object to the specified offset.

     DEFAULT(whence) = 0
     */
	 uint64_t(*seek) (FileLikeObject self, int64_t offset, int whence);
	int (*read) (FileLikeObject self, char *buffer,
		     unsigned long int length);
	/*
	   A variant of read above that will read upto the next \r or
	   \r\n.

	   DEFAULT(length) = 1024
	 */
	int (*readline) (FileLikeObject self, char *buffer,
			 unsigned long int length);
	int (*write) (FileLikeObject self, char *buffer,
		      unsigned long int length);
	 uint64_t(*tell) (FileLikeObject self);
	// This can be used to get the content of the FileLikeObject in a
	// big buffer of data. The data will be cached with the
	// FileLikeObject. Its only really suitable for smallish amounts of
	// data - and checks to ensure that file size is less than MAX_CACHED_FILESIZE
	char *(*get_data) (FileLikeObject self);
// This method is just like the standard ftruncate call
	int (*truncate) (FileLikeObject self, uint64_t offset);
// This closes the FileLikeObject and also frees it - it is not valid
// to use the FileLikeObject after calling this (it gets free'd).
//     DESTRUCTOR int METHOD(FileLikeObject, close);
};

// This file like object is backed by a real disk file:
typedef struct FileBackedObject_t *FileBackedObject;

FileBackedObject alloc_FileBackedObject();	/* Allocates object memory */

int FileBackedObject_init(Object self);	/* Class initializer */

extern struct FileBackedObject_t __FileBackedObject;	/* Public class template */

struct FileBackedObject_t {
	struct FileLikeObject_t super;	/* Superclass Fields we inherit */
	FileBackedObject __class__;	/* Pointer to our own class */
	FileLikeObject __super__;	/* Pointer to our superclass */
	int fd;
};

;

     /** This is an abstract class that implements AFF4 volumes */
typedef struct AFF4Volume_t *AFF4Volume;

AFF4Volume alloc_AFF4Volume();	/* Allocates object memory */

int AFF4Volume_init(Object self);	/* Class initializer */

extern struct AFF4Volume_t __AFF4Volume;	/* Public class template */

struct AFF4Volume_t {
	struct AFFObject_t super;	/* Superclass Fields we inherit */
	AFF4Volume __class__;	/* Pointer to our own class */
	AFFObject __super__;	/* Pointer to our superclass */
// This method opens an existing member or creates a new one. We
// return a file like object which may be used to read and write the
// member. If we open a member for writing the zip file will be locked
// (so another attempt to open a new member for writing will raise,
// until this member is promptly closed). The ZipFile must have been
// called with create_new_volume or append_volume before.
//
// DEFAULT(mode) = "r";
// DEFAULT(compression) = ZIP_DEFLATE;
	 FileLikeObject(*open_member) (AFF4Volume self, char *filename,
				       char mode, uint16_t compression);
// This method flushes the central directory and finalises the
// file. The file may still be accessed for reading after this.
//     DESTRUCTOR int METHOD(AFF4Volume, close);
// A convenience function for storing a string as a new file (it
// basically calls open_member, writes the string then closes it).
//
// DEFAULT(compression) = ZIP_DEFLATE
	int (*writestr) (AFF4Volume self, char *filename, char *data, int len,
			 uint16_t compression);
	/*
	   Load an AFF4 volume from the URN specified. We parse all the RDF
	   serializations.

	   DEFAULT(mode) = "r"
	 */
	int (*load_from) (AFF4Volume self, RDFURN fd_urn, char mode);
};

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

/* This is a function that will be run when the library is imported. It
   should be used to initialise static stuff.

   It will only be called once. If you require some other subsystem to
   be called first you may call it from here.
*/
struct SecurityProvider_t;

extern int AFF4_TDB_FLAGS;

/* The TDB_DATA_LIST flags */

/* This signals that the RDFValue entry has the same encoding and
   serialised form. This allows us to do some optimizations.
*/

/* The data store is basically a singly linked list of these records: */
typedef struct TDB_DATA_LIST {
	uint64_t next_offset;
	uint16_t length;
	/*
	   This is the urn_id of the object which asserted this
	   statement. Can be 0 to indicate unknown.
	 */
	int32_t asserter_id;
	/*
	   This type refers to the full name type as stored in types.tdb 
	 */
	uint8_t encoding_type;
	/*
	   This field contains various flags about this entry 
	 */
	uint8_t flags;
} __attribute__ ((packed)) TDB_DATA_LIST;

/** This object is returned when iterating a result set from the
    resolver. Its basically a pointer into the resolver data store.*/
typedef struct RESOLVER_ITER {
	TDB_DATA_LIST head;
	uint64_t offset;
	/*
	   This is used to ensure we do not iterate over multiple values
	   which are the same (Cache is basically used as a set here).
	 */
	Cache cache;
	RDFURN urn;
} RESOLVER_ITER;

;

/** The resolver is at the heart of the AFF4 specification - it is
    responsible for returning objects keyed by attribute from a
    globally unique identifier (URI) and managing the central
    information store.
*/
typedef struct Resolver_t *Resolver;

Resolver alloc_Resolver();	/* Allocates object memory */

int Resolver_init(Object self);	/* Class initializer */

extern struct Resolver_t __Resolver;	/* Public class template */

struct Resolver_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	Resolver __class__;	/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */

/* These are the various TDB files we have open */
	struct tdb_context *urn_db;
	struct tdb_context *attribute_db;
	struct tdb_context *data_db;
	/*
	   This descriptor is for the data.tdb store (which is not a tdb file btw) 
	 */
	int data_store_fd;
	/*
	   The mode we opened the Resolver with 
	 */
	int mode;

       /** This is used to restore state if the RDF parser fails */
	jmp_buf env;
	char *message;
	/*
	   Read and write caches. These have different policies. The
	   read cache is just for efficiency - if an object is not in
	   the cache or is used by another thread, we just create a new
	   one of those.
	 */
	Cache read_cache;
	/*
	   Write cache is used for locks - it is not possible to have
	   multiple write objects at the same time, and all writers are
	   opened exclusively. Attempting to open an already locked
	   object for writing will block until that object is returned
	   to the cache.
	 */
	Cache write_cache;
	/*
	   This mutex protects our internal data structures 
	 */
	pthread_mutex_t mutex;
	/*
	   This is used to check the type of new objects 
	 */
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
	 Resolver(*Con) (Resolver self, int mode);

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
	 AFFObject(*open) (Resolver self, RDFURN uri, char mode);
	/*
	   All objects obtained from Resolver.open() need to be
	   returned to the cache promptly using this method. NOTE - it
	   is an error to be using an object after being returned to
	   the cache - it might not be valid and may be gced.

	   NOTE for Python bindings - do not use this function, instead
	   use AFFObject.cache_return() method which ensures the
	   wrapped python object is properly destroyed after this call.
	 */
	void (*cache_return) (Resolver self, AFFObject obj);
	/*
	   This create a new object of the specified type.

	   name specifies the type of object as registered in the type
	   handler dispatcher. (e.g. AFF4_ZIP_VOLUME)

	   DEFAULT(mode) = "w"
	 */
	 AFFObject(*create) (Resolver self, char *name, char mode);
	/*
	   This function resolves the value in uri and attribute and sets it
	   into the RDFValue object. So you must first create such an object
	   (e.g. XSDDatetime) and then pass the object here to be updated
	   from the data store. Note that only a single value is returned -
	   if you want to iterate over all the values for this attribute you
	   need to call get_iter and iter_next.
	 */
	int (*resolve_value) (Resolver self, RDFURN uri, char *attribute,
			      RDFValue value);

       /** Similar to Resolver.resolve_value, but a new RDFValue is
           allocated with the context provided. */
	 RDFValue(*resolve_alloc) (Resolver self, void *ctx, RDFURN uri,
				   char *attribute);
	/*
	   This is a version of the above which uses an iterator to iterate
	   over the list.

	   The iterator is obtained using get_iter first. This function
	   returns 1 if an iterator can be found (i.e. at least one result
	   exists) or 0 if no results exist.

	   Each call to iter_next will write a new value into the buffer set
	   up by result with maximal length length. Only results matching
	   the type specified are returned. We return length written for
	   each successful iteration, and zero when we have no more items.
	 */
	RESOLVER_ITER *(*get_iter) (Resolver self, void *ctx, RDFURN uri,
				    char *attribute);
	/*
	   This method reads the next result from the iterator. result
	   must be an allocated and valid RDFValue object 
	 */
	int (*iter_next) (Resolver self, RESOLVER_ITER * iter, RDFValue result);
	/*
	   This method is similar to iter_next except the result is
	   allocated to the NULL context. Callers need to talloc_free
	   the result. This advantage of this method is that we dont
	   need to know in advance what type the value is.

	   Note - this function advances the iterator.
	 */
	 RDFValue(*alloc_from_iter) (Resolver self, void *ctx,
				     RESOLVER_ITER * iter);
	/*
	   This is a shortcut method for retrieving the encoded version
	   from the iterator.

	   Note - this function advances the iterator.
	 */
	char *(*encoded_data_from_iter) (Resolver self,
					 RDFValue * rdf_value_class,
					 RESOLVER_ITER * iter);
	/*
	   Deletes all values for this attribute from the resolver

	   DEFAULT(attribute) = NULL;
	 */
	void (*del) (Resolver self, RDFURN uri, char *attribute);
	/*
	   Expires this object and all objects it owns.

	   NOTE: any outstanding objects will become invalidated. For
	   example, if you expire a volume, any outstanding streams
	   opened within the volume will become invalidated.
	 */
	void (*expire) (Resolver self, RDFURN uri);
	/*
	   Sets a new value for an attribute. Note that this function
	   clears any previously set values, if you want to create a list
	   of values you need to call add_value.

	   If iter is given the iterator will be filled into it. The
	   iterator can subsequently be used with iter_alloc() to
	   reobtain the value.

	   DEFAULT(iter) = NULL;
	 */
	int (*set_value) (Resolver self, RDFURN uri, char *attribute,
			  RDFValue value, RESOLVER_ITER * iter);
	/*
	   Adds a new value to the value list for this attribute.

	   DEFAULT(iter) = NULL;
	 */
	int (*add_value) (Resolver self, RDFURN uri, char *attribute,
			  RDFValue value, RESOLVER_ITER * iter);

       /** This function can be used to iterate over all the
           attributes set for a subject. For each attribute, the
           RESOLVER_ITER is set to point to the start of its attribute
           run.

           attribute is set to the value of the attribute which was returned.

           We return 0 if no further attributes are available.
       */
	int (*attributes_iter) (Resolver self, RDFURN urn, XSDString attribute,
				RESOLVER_ITER * iter);

       /** This returns a unique ID for the given URN. The ID is only
           unique within this resolver.

           DEFAULT(create) = 0
       */
	int (*get_id_by_urn) (Resolver self, RDFURN uri, int create);

       /** This fills the URI specified by id into the uri container
       passed. Returns 1 if the ID is found, or 0 if the ID is not
       found.

       RAISES(func_return == 0, KeyError) = "URI not found for id %llu", (long long unsigned int)id
       **/
	int (*get_urn_by_id) (Resolver self, int id, RDFURN uri);

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
	void (*register_rdf_value_class) (Resolver self, RDFValue class_ref);

       /** This is a handler for new types - types get registered here */
	void (*register_type_dispatcher) (Resolver self, char *type,
					  AFFObject class_ref);
	void (*register_security_provider) (Resolver self,
					    struct SecurityProvider_t *
					    class_ref);
	/*
	   This can be used to install a logger object. All messages
	   will then be output through this object.
	 */
	void (*register_logger) (Resolver self, Logger logger);

       /** A Utility method to create a new instance of a registered
           RDF dataType.
       */
	 RDFValue(*new_rdfvalue) (Resolver self, void *ctx, char *type);
	/*
	   This function attempts to load the volume stored within the
	   FileLikeObject URI provided. If there is a volume, the
	   volume URI is set in uri, and we return true.

	   We attempt to instantiate all volume drivers in turn until
	   one works and then load them from the URI.
	 */
	int (*load) (Resolver self, RDFURN uri);

       /** A generic interface to the logger allows any code to send
           messages to the provided logger.
       */
	void (*log) (Resolver self, int level, char *service, RDFURN subject,
		     char *messages);

       /** This closes and frees all memory used by the resolver.

           This is generally needed after forking as two resolvers can
           not exist in different processes.
       */
	void (*close) (Resolver self);

       /** This is used to flush all our caches */
	void (*flush) (Resolver self);
};

       /*
          This is just a wrapper around tdb so it can easily be used
          from python etc. 
        */
typedef struct TDB_t *TDB;

TDB alloc_TDB();		/* Allocates object memory */

int TDB_init(Object self);	/* Class initializer */

extern struct TDB_t __TDB;	/* Public class template */

struct TDB_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	TDB __class__;		/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */
	struct tdb_context *file;

     /** This is the constructor for a new TDB database.

         DEFAULT(mode) = 0;
     */
	 TDB(*Con) (TDB self, char *filename, int mode);
	void (*store) (TDB self, char *key, int key_len, char *data, int len);
	 TDB_DATA(*fetch) (TDB self, char *key, int len);
};

     /*
        This function is the main entry point into the AFF4
        library. The first thing that users need to do is obtain a
        resolver by calling AFF4_get_resolver(). Once they have a
        resolver, new instances of AFF4 objects and RDFValue objects
        can be obtained throught the open(), create(), and
        new_rdfvalue() method.
      */
Resolver AFF4_get_resolver();

     /*
        This function can be used to register a new AFF4 type with the
        library
      */
void register_type_dispatcher(Resolver self, char *type, AFFObject * class_ref);

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

     /*
        This function is used for debugging only. We use it to free
        miscelaneous objects which should never be freed. Never call
        this in production.
      */
void aff4_end();

/*
** aff4_rdf_serialise.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Wed Nov 18 12:36:21 2009 mic
** Last update Fri Jan  8 15:53:23 2010 mic
*/

/***** Following is an implementation of a serialiser */
typedef struct RDFSerializer_t *RDFSerializer;

RDFSerializer alloc_RDFSerializer();	/* Allocates object memory */

int RDFSerializer_init(Object self);	/* Class initializer */

extern struct RDFSerializer_t __RDFSerializer;	/* Public class template */

struct RDFSerializer_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	RDFSerializer __class__;	/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */
	raptor_serializer *rdf_serializer;
	raptor_iostream *iostream;
	FileLikeObject fd;
	int count;
	char buff[BUFF_SIZE * 2];
	int i;
	Cache attributes;
	 RDFSerializer(*Con) (RDFSerializer self, char *base_urn,
			      FileLikeObject fd);
	int (*serialize_urn) (RDFSerializer self, RDFURN urn);
	int (*serialize_statement) (RDFSerializer self, RESOLVER_ITER * iter,
				    RDFURN urn, RDFURN attribute);
	void (*set_namespace) (RDFSerializer self, char *prefix,
			       char *namespace);
	void (*close) (RDFSerializer self);
};

typedef struct RDFParser_t *RDFParser;

RDFParser alloc_RDFParser();	/* Allocates object memory */

int RDFParser_init(Object self);	/* Class initializer */

extern struct RDFParser_t __RDFParser;	/* Public class template */

struct RDFParser_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	RDFParser __class__;	/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */
	char message[BUFF_SIZE];
	jmp_buf env;
	RDFURN urn;
	RDFURN volume_urn;
	Cache member_cache;
	void (*triples_handler) (void *self, const raptor_statement * triple);
	void (*message_handler) (void *self, raptor_locator * locator,
				 const char *message);
// Parses data stored in fd using the format specified. fd is assumed
// to contain a base URN specified (or NULL if non specified).
	int (*parse) (RDFParser self, FileLikeObject fd, char *format,
		      char *base);
	 RDFParser(*Con) (RDFParser self);
};

/** A Graph is a named collection of RDF statements about various
    objects.

    When the graph is stored it will serialise into a segment with all
    the statements contained with it using the appropriate RDF
    serialization.
*/
typedef struct Graph_t *Graph;

Graph alloc_Graph();		/* Allocates object memory */

int Graph_init(Object self);	/* Class initializer */

extern struct Graph_t __Graph;	/* Public class template */

struct Graph_t {
	struct AFFObject_t super;	/* Superclass Fields we inherit */
	Graph __class__;	/* Pointer to our own class */
	AFFObject __super__;	/* Pointer to our superclass */
	RDFURN stored;
	RDFURN attribute_urn;
	XSDString statement;

  /** Add a new triple to this Graph */
	void (*set_triple) (Graph self, RDFURN subject, char *attribute,
			    RDFValue value);
};

/* This is the worker object itself (private) */
struct ImageWorker_t;

/** The Image Stream represents an Image in chunks */
typedef struct Image_t *Image;

Image alloc_Image();		/* Allocates object memory */

int Image_init(Object self);	/* Class initializer */

extern struct Image_t __Image;	/* Public class template */

struct Image_t {
	struct FileLikeObject_t super;	/* Superclass Fields we inherit */
	Image __class__;	/* Pointer to our own class */
	FileLikeObject __super__;	/* Pointer to our superclass */

/* This is where the image is stored */
	RDFURN stored;
	/*
	   These are the URNs for the bevy and the bevy index 
	 */
	RDFURN bevy_urn;
	/*
	   Chunks are cached here for faster randoom reading performance.
	 */
	Cache chunk_cache;
	/*
	   This is a queue of all workers available 
	 */
	Queue workers;
	/*
	   This is a queue of all workers busy 
	 */
	Queue busy;
	/*
	   Thats the current worker we are using - when it gets full, we
	   simply dump its bevy and take a new worker here.
	 */
	struct ImageWorker_t *current;

  /** Some parameters about this image */
	XSDInteger chunk_size;
	XSDInteger compression;
	XSDInteger chunks_in_segment;
	uint32_t bevy_size;
	int segment_count;
	EVP_MD_CTX digest;

  /** This sets the number of working threads.

      The default number of threads is zero (no threads). Set to a
      higher number to utilize multiple threads here.
  */
	void (*set_workers) (Image self, int workers);
};

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

    Optional properties:

    - file_period - number of bytes in the file offset which this map
      repreats on. (Useful for RAID)

    - image_period - number of bytes in the target image each period
      will advance by. (Useful for RAID)
*/
struct map_point_tree_s;

typedef struct MapValue_t *MapValue;

MapValue alloc_MapValue();	/* Allocates object memory */

int MapValue_init(Object self);	/* Class initializer */

extern struct MapValue_t __MapValue;	/* Public class template */

struct MapValue_t {
	struct RDFValue_t super;	/* Superclass Fields we inherit */
	MapValue __class__;	/* Pointer to our own class */
	RDFValue __super__;	/* Pointer to our superclass */
	// The urn of the map object we belong to
	RDFURN urn;
	XSDInteger size;
	int number_of_points;
	int number_of_urns;
	// This is where we store the node treap
	struct map_point_tree_s *tree;
	// An array of all the targets we know about
	RDFURN *targets;
	Cache cache;
	// The period offsets repeat within each target
	XSDInteger target_period;
	// The period offsets repear within the logical image
	XSDInteger image_period;
	// The blocksize is a constant multiple for each map offset.
	XSDInteger blocksize;
	 uint64_t(*add_point) (MapValue self, uint64_t image_offset,
			       uint64_t target_offset, char *target);
	/*
	   This function returns information about the current file pointer
	   and its view of the target slice.

	   DEFAULT(target_idx) = NULL;
	 */
	 RDFURN(*get_range) (MapValue self, uint64_t readptr,
			     uint64_t * target_offset_at_point,
			     uint64_t * available_to_read,
			     uint32_t * target_idx);
};

// Some alternative implementations
typedef struct MapValueBinary_t *MapValueBinary;

MapValueBinary alloc_MapValueBinary();	/* Allocates object memory */

int MapValueBinary_init(Object self);	/* Class initializer */

extern struct MapValueBinary_t __MapValueBinary;	/* Public class template */

struct MapValueBinary_t {
	struct MapValue_t super;	/* Superclass Fields we inherit */
	MapValueBinary __class__;	/* Pointer to our own class */
	MapValue __super__;	/* Pointer to our superclass */
};

// Sometimes its quicker to just serialise the map inline
typedef struct MapValueInline_t *MapValueInline;

MapValueInline alloc_MapValueInline();	/* Allocates object memory */

int MapValueInline_init(Object self);	/* Class initializer */

extern struct MapValueInline_t __MapValueInline;	/* Public class template */

struct MapValueInline_t {
	struct MapValue_t super;	/* Superclass Fields we inherit */
	MapValueInline __class__;	/* Pointer to our own class */
	MapValue __super__;	/* Pointer to our superclass */
	char *buffer;
	int i;
};

typedef struct MapDriver_t *MapDriver;

MapDriver alloc_MapDriver();	/* Allocates object memory */

int MapDriver_init(Object self);	/* Class initializer */

extern struct MapDriver_t __MapDriver;	/* Public class template */

struct MapDriver_t {
	struct FileLikeObject_t super;	/* Superclass Fields we inherit */
	MapDriver __class__;	/* Pointer to our own class */
	FileLikeObject __super__;	/* Pointer to our superclass */
	MapValue map;
	// This where we get stored
	RDFURN stored;
	RDFURN target_urn;
	XSDInteger dirty;
	// This flag indicates if we should automatically set the most
	// optimal map implementation
	int custom_map;
	// Sets the data type of the map object
	void (*set_data_type) (MapDriver self, char *type);
	// Deletes the point at the specified file offset
	void (*del) (MapDriver self, uint64_t target_pos);
	// Adds a new point to the file offset table
	void (*add_point) (MapDriver self, uint64_t image_offset,
			   uint64_t target_offset, char *target);
	/*
	   This interface is a more natural way to build the map, by
	   simulating copying from different offsets in different targets
	   sequentially. This function will add a new map point, and advance
	   the readptr by the specified length.
	 */
	void (*write_from) (MapDriver self, RDFURN target,
			    uint64_t target_offset, uint64_t length);
	void (*save_map) (MapDriver self);
};

;
;
  // This is the largest file size which may be represented by a
  // regular zip file without using Zip64 extensions.
  //#define ZIP64_LIMIT 1

/** These are ZipFile structures */
struct EndCentralDirectory {
	uint32_t magic;
	uint16_t number_of_this_disk;
	uint16_t disk_with_cd;
	uint16_t total_entries_in_cd_on_disk;
	uint16_t total_entries_in_cd;
	uint32_t size_of_cd;
	uint32_t offset_of_cd;
	uint16_t comment_len;
} __attribute__ ((packed));

/** As we parse these fields we populate the oracle */
struct CDFileHeader {
	uint32_t magic;
	uint16_t version_made_by;
	uint16_t version_needed;
	uint16_t flags;
	uint16_t compression_method;	/* aff2volatile:compression */
	uint16_t dostime;	/* aff2volatile:timestamp */
	uint16_t dosdate;
	uint32_t crc32;
	uint32_t compress_size;	/* aff2volatile:compress_size */
	uint32_t file_size;	/* aff2volatile:file_size */
	uint16_t file_name_length;
	uint16_t extra_field_len;
	uint16_t file_comment_length;
	uint16_t disk_number_start;
	uint16_t internal_file_attr;
	uint32_t external_file_attr;
	uint32_t relative_offset_local_header;	/* aff2volatile:header_offset */
} __attribute__ ((packed));

struct ZipFileHeader {
	uint32_t magic;
	uint16_t version;
	uint16_t flags;
	uint16_t compression_method;
	uint16_t lastmodtime;
	uint16_t lastmoddate;
	uint32_t crc32;
	uint32_t compress_size;
	uint32_t file_size;
	uint16_t file_name_length;
	uint16_t extra_field_len;
} __attribute__ ((packed));

struct Zip64EndCD {
	uint32_t magic;
	uint64_t size_of_header;
	uint16_t version_made_by;
	uint16_t version_needed;
	uint32_t number_of_disk;
	uint32_t number_of_disk_with_cd;
	uint64_t number_of_entries_in_volume;
	uint64_t number_of_entries_in_total;
	uint64_t size_of_cd;
	uint64_t offset_of_cd;
} __attribute__ ((packed));

struct Zip64CDLocator {
	uint32_t magic;
	uint32_t disk_with_cd;
	uint64_t offset_of_end_cd;
	uint32_t number_of_disks;
} __attribute__ ((packed));

/** This represents a Zip file */
typedef struct ZipFile_t *ZipFile;

ZipFile alloc_ZipFile();	/* Allocates object memory */

int ZipFile_init(Object self);	/* Class initializer */

extern struct ZipFile_t __ZipFile;	/* Public class template */

struct ZipFile_t {
	struct AFF4Volume_t super;	/* Superclass Fields we inherit */
	ZipFile __class__;	/* Pointer to our own class */
	AFF4Volume __super__;	/* Pointer to our superclass */
	// This keeps the end of central directory struct so we can
	// recopy it when we update the CD.
	struct EndCentralDirectory *end;
	// The following are attributes stored by various zip64 extended
	// fields
	uint64_t total_entries;
	uint64_t original_member_size;
	uint64_t compressed_member_size;
	uint64_t offset_of_member_header;

  /** Some commonly used RDF types */
	XSDInteger directory_offset;
	RDFURN storage_urn;
	XSDInteger _didModify;

  /** attributes of files used for the EndCentralDirectory */
	XSDInteger type;
	XSDInteger epoch_time;
	XSDInteger compression_method;
	XSDInteger crc;
	XSDInteger size;
	XSDInteger compressed_size;
	XSDInteger header_offset;
};

// This is a FileLikeObject which is used to provide access to zip
// archive members. Currently only accessible through
// ZipFile.open_member()
typedef struct ZipFileStream_t *ZipFileStream;

ZipFileStream alloc_ZipFileStream();	/* Allocates object memory */

int ZipFileStream_init(Object self);	/* Class initializer */

extern struct ZipFileStream_t __ZipFileStream;	/* Public class template */

struct ZipFileStream_t {
	struct FileLikeObject_t super;	/* Superclass Fields we inherit */
	ZipFileStream __class__;	/* Pointer to our own class */
	FileLikeObject __super__;	/* Pointer to our superclass */
	z_stream strm;
	XSDInteger file_offset;
	// The file backing the container
	RDFURN file_urn;
	// The container ZIP file we are written in
	RDFURN container_urn;
	FileLikeObject file_fd;
	XSDInteger crc32;
	XSDInteger compress_size;
	XSDInteger compression;
	XSDInteger dirty;
	// For now we just decompress the entire segment into memory if
	// required
	char *cbuff;
	char *buff;
	// We calculate the SHA256 hash of each archive member
	EVP_MD_CTX digest;
	/*
	   This is the constructor for the file like object. 
	   file_urn is the storage file for the volume in
	   container_urn. If the stream is opened for writing the file_fd
	   may be passed in. It remains locked until we are closed.
	 */
	 ZipFileStream(*Con2) (ZipFileStream self, RDFURN urn, RDFURN file_urn,
			       RDFURN container_urn, char mode,
			       FileLikeObject file_fd);
};

// A directory volume implementation - all elements live in a single
// directory structure
typedef struct DirVolume_t *DirVolume;

DirVolume alloc_DirVolume();	/* Allocates object memory */

int DirVolume_init(Object self);	/* Class initializer */

extern struct DirVolume_t __DirVolume;	/* Public class template */

struct DirVolume_t {
	struct ZipFile_t super;	/* Superclass Fields we inherit */
	DirVolume __class__;	/* Pointer to our own class */
	ZipFile __super__;	/* Pointer to our superclass */
};

/** This is a dispatcher of stream classes depending on their name.
*/
struct dispatch_t {
	// A boolean to determine if this is a scheme or a type
	int scheme;
	char *type;
	AFFObject class_ptr;
};

extern struct dispatch_t dispatch[];

extern struct dispatch_t volume_handlers[];

void dump_stream_properties(FileLikeObject self);

//Some useful utilities
ZipFile open_volume(char *urn);

  // Members in a volume may have a URN relative to the volume
  // URN. This is still globally unique, but may be stored in a more
  // concise form. For example the ZipFile member "default" is a
  // relative name with a fully qualified name of
  // Volume_URN/default. These functions are used to convert from
  // fully qualified to relative names as needed. The result is a
  // static buffer.
  //char *fully_qualified_name(void *ctx, char *name, RDFURN volume_urn);
  //char *relative_name(void *ctx, char *name, RDFURN volume_urn);
void zip_init();

void image_init();

void mapdriver_init();

void graph_init();

/*
** aff4_crypto.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Wed Dec 16 13:48:12 2009 mic
** Last update Wed Dec 16 13:48:30 2009 mic
*/

/** This class implements a security provider. This is used by AFF4
    ciphers to request passwords or certificates. Users of the library
    should implement their own security provider and install it using
    the Resolver.install_security_provider() function. When a cipher
    needs to obtain a passphrase or certificate they call here to
    obtain it - the user code can choose how to manage their own
    password (e.g. bring up an interactive GUI, use the OS secure
    storage facility etc).

    There is currently only a single global security provider.
*/
typedef struct SecurityProvider_t *SecurityProvider;

SecurityProvider alloc_SecurityProvider();	/* Allocates object memory */

int SecurityProvider_init(Object self);	/* Class initializer */

extern struct SecurityProvider_t __SecurityProvider;	/* Public class template */

struct SecurityProvider_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	SecurityProvider __class__;	/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */
	 SecurityProvider(*Con) (SecurityProvider self);
	/*
	   This is called when we need to obtain a passphrase for a subject
	   stream. Returns 0 on error, and the length of the passphrase in
	   len (which must initially contain the length of the passphrase
	   buffer).
	 */
	char *(*passphrase) (SecurityProvider self, char *cipher_type,
			     struct RDFURN_t * subject);
	/*
	   This method should return the location (URL) of the
	   certificate that will be used to encrypt the data.
	 */
	char *(*x509_private_key) (SecurityProvider self, char *cert_name,
				   struct RDFURN_t * subject);
};

;

     /** This is a static pointer for the global security provider */
SecurityProvider AFF4_SECURITY_PROVIDER;

/************************************************************
  An implementation of the encrypted Stream.

  This stream encrypts and decrypts data from a target stream in
  blocks determined by the "block_size" attribute. The IV consists of
  the block number (as 32 bit int in little endian) appended to an 8
  byte salt.

  Encryption is controlled through an AFF4Cipher object which is
  responsible for encryption and decryption of blocks. The AFF4Cipher
  is an RDFValue and therefore gets serialised in the RDF infomation
  space. Many AFF4Ciphers may be implemented to encrypt/decrypt the
  same Encrypted stream. This allows multiple keys, to be employed.
*************************************************************/
typedef struct Key_t *Key;

Key alloc_Key();		/* Allocates object memory */

int Key_init(Object self);	/* Class initializer */

extern struct Key_t __Key;	/* Public class template */

struct Key_t {
	struct Object_t super;	/* Superclass Fields we inherit */
	Key __class__;		/* Pointer to our own class */
	Object __super__;	/* Pointer to our superclass */
	char *type;
	TDB_DATA data;
	TDB_DATA iv;

    /** Create a new key for the subject specified of the type
        specified.

        If create is specified we create a new key if it is not found
        in the cache.
    */
	 Key(*Con) (Key self, char *type, RDFURN subject, int create);
};

/** This is the abstract cipher class - all ciphers must implement
    these methods.
*/
typedef struct AFF4Cipher_t *AFF4Cipher;

AFF4Cipher alloc_AFF4Cipher();	/* Allocates object memory */

int AFF4Cipher_init(Object self);	/* Class initializer */

extern struct AFF4Cipher_t __AFF4Cipher;	/* Public class template */

struct AFF4Cipher_t {
	struct RDFValue_t super;	/* Superclass Fields we inherit */
	AFF4Cipher __class__;	/* Pointer to our own class */
	RDFValue __super__;	/* Pointer to our superclass */
	int blocksize;
	/*
	   A type string which identifies the cipher 
	 */
	char *type;
	/*
	   This contains the key for this cipher. Most ciphers will
	   encrypt this key in some way upon being serialised.
	 */
	Key master_key;
	int (*encrypt) (AFF4Cipher self, int chunk_number,
			unsigned char *inbuff, unsigned long int inlen,
			unsigned char *outbuff, unsigned long int length);
	int (*decrypt) (AFF4Cipher self, int chunk_number,
			unsigned char *inbuff, unsigned long int inlen,
			unsigned char *outbuff, unsigned long int length);
};

struct aff4_cipher_data_t;

/** The following are some default ciphers */

/**
   This cipher uses AES256. The session key is derived from a
   password using the PBKDF2 algorithm. The round number is derived
   from the iv: round_number = (unsigned char)iv[0] << 8

   The key is obtained from PKCS5_PBKDF2_HMAC_SHA1(password, iv, round_number).

   Password is obtained from the security manager.

   The serialised form is a base64 encoded version struct
   aff4_cipher_data_t (above). Where the nonce is the iv encrypted
   using the key.
**/
typedef struct AES256Password_t *AES256Password;

AES256Password alloc_AES256Password();	/* Allocates object memory */

int AES256Password_init(Object self);	/* Class initializer */

extern struct AES256Password_t __AES256Password;	/* Public class template */

struct AES256Password_t {
	struct AFF4Cipher_t super;	/* Superclass Fields we inherit */
	AES256Password __class__;	/* Pointer to our own class */
	AFF4Cipher __super__;	/* Pointer to our superclass */
	AES_KEY ekey;
	AES_KEY dkey;
};

/**
   This cipher uses AES256.

   The serialised form is the url of the x509 certificate that was
   used to encrypt the key, with the base64 encoded struct
   aff4_cipher_data_t being encoded as the query string. The security
   manager is used to get both the certificate and the private keys.
**/
typedef struct AES256X509_t *AES256X509;

AES256X509 alloc_AES256X509();	/* Allocates object memory */

int AES256X509_init(Object self);	/* Class initializer */

extern struct AES256X509_t __AES256X509;	/* Public class template */

struct AES256X509_t {
	struct AES256Password_t super;	/* Superclass Fields we inherit */
	AES256X509 __class__;	/* Pointer to our own class */
	AES256Password __super__;	/* Pointer to our superclass */

/**
    This method is used to set the authority (X509 certificate) of
    this object. The location is a URL for the X509 certificate.

    return 1 on success, 0 on failure.
**/
	X509 *authority;
	RDFURN location;
	/*
	   This method sets the X509 certificate with which the master
	   key is encrypted.
	 */
	int (*set_authority) (AES256X509 self, RDFURN location);
};

/**
   Encryption is handled via two components, the encrypted stream and
   the cipher. An encrypted stream is an FileLikeObject with the
   following AFF4 attributes:

   AFF4_CHUNK_SIZE = Data will be broken into these chunks and
                     encrypted independantly.

   AFF4_STORED     = Data will be stored on this backing stream.

   AFF4_CIPHER     = This is an RDFValue which extends the AFF4Cipher
                     class. More on that below.

   When opened, the FileLikeObject is created by instantiating a
   cipher from the AFF4_CIPHER attribute. Data is then divided into
   chunks and each chunk is encrypted using the cipher, and then
   written to the backing stream.

   A cipher is a class which extends the AFF4Cipher baseclass.

   Of course a valid cipher must also implement valid serialization
   methods and also some way of key initialization. Chunks must be
   whole multiples of blocksize.
*/
typedef struct Encrypted_t *Encrypted;

Encrypted alloc_Encrypted();	/* Allocates object memory */

int Encrypted_init(Object self);	/* Class initializer */

extern struct Encrypted_t __Encrypted;	/* Public class template */

struct Encrypted_t {
	struct FileLikeObject_t super;	/* Superclass Fields we inherit */
	Encrypted __class__;	/* Pointer to our own class */
	FileLikeObject __super__;	/* Pointer to our superclass */
	StringIO block_buffer;
	AFF4Cipher cipher;
	RDFURN backing_store;
	RDFURN stored;
	XSDInteger chunk_size;
};

/*
** aff4_errors.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Sat Mar  6 20:54:25 2010 mic
** Last update Sat Mar  6 20:54:25 2010 mic
*/
// Some helpful little things

/** This is used for error reporting. This is similar to the way
    python does it, i.e. we set the error flag and return NULL.
*/
enum _error_type {
	EZero, EGeneric, EOverflow, EWarning,
	EUnderflow, EIOError, ENoMemory, EInvalidParameter, ERuntimeError,
	    EKeyError,
	// Reserved for impossible conditions
	EProgrammingError
};

void *aff4_raise_errors(enum _error_type t, char *string, ...);

/** We only set the error state if its not already set */

/** The current error state is returned by this function.

    This is done in a thread safe manner.
 */
enum _error_type *aff4_get_current_error(char **error_str);

// These macros are used when we need to do something which might
// change the error state on the error path of a function.

/************************** Class AFF4_MapValueBinary **************************
 Some alternative implementations

************************** Methods  **************************/
uint64_t aff4_MapValueBinary_parse(MapValueBinary self, char *serialised_form,
				   RDFURN subject);
TDB_DATA *aff4_MapValueBinary_encode(MapValueBinary self, RDFURN subject);

uint64_t aff4_MapValueBinary_decode(MapValueBinary self, char *data, int length,
				    RDFURN subject);
char *aff4_MapValueBinary_serialise(MapValueBinary self, RDFURN subject);

RDFValue aff4_MapValueBinary_clone(MapValueBinary self);

uint64_t aff4_MapValueBinary_add_point(MapValueBinary self,
				       uint64_t image_offset,
				       uint64_t target_offset, char *target);
RDFURN aff4_MapValueBinary_get_range(MapValueBinary self, uint64_t readptr,
				     uint64_t * target_offset_at_point,
				     uint64_t * available_to_read,
				     uint32_t * target_idx);

/************************** Class AFF4_MapDriver **************************

************************** Methods  **************************/
void aff4_MapDriver_set(MapDriver self, char *attribute, RDFValue value);

void aff4_MapDriver_add(MapDriver self, char *attribute, RDFValue value);

AFFObject aff4_MapDriver_finish(MapDriver self);

void aff4_MapDriver_cache_return(MapDriver self);

uint64_t aff4_MapDriver_close(MapDriver self);

uint64_t aff4_MapDriver_seek(MapDriver self, int64_t offset, int whence);

uint64_t aff4_MapDriver_read(MapDriver self, char *buffer,
			     unsigned long int length);
uint64_t aff4_MapDriver_readline(MapDriver self, char *buffer,
				 unsigned long int length);
uint64_t aff4_MapDriver_write(MapDriver self, char *buffer,
			      unsigned long int length);
uint64_t aff4_MapDriver_tell(MapDriver self);

char *aff4_MapDriver_get_data(MapDriver self);

uint64_t aff4_MapDriver_truncate(MapDriver self, uint64_t offset);

/*
Sets the data type of the map object
*/
void aff4_MapDriver_set_data_type(MapDriver self, char *type);

/*
Deletes the point at the specified file offset
*/
void aff4_MapDriver_del(MapDriver self, uint64_t target_pos);

/*
Adds a new point to the file offset table
*/
void aff4_MapDriver_add_point(MapDriver self, uint64_t image_offset,
			      uint64_t target_offset, char *target);

/*
This interface is a more natural way to build the map, by
     simulating copying from different offsets in different targets
     sequentially. This function will add a new map point, and advance
     the readptr by the specified length.
*/
void aff4_MapDriver_write_from(MapDriver self, RDFURN target,
			       uint64_t target_offset, uint64_t length);
void aff4_MapDriver_save_map(MapDriver self);

/************************** Class AFF4_AES256Password **************************
 The following are some default ciphers 
   This cipher uses AES256. The session key is derived from a
   password using the PBKDF2 algorithm. The round number is derived
   from the iv: round_number = (unsigned char)iv[0] << 8

   The key is obtained from PKCS5_PBKDF2_HMAC_SHA1(password, iv, round_number).

   Password is obtained from the security manager.

   The serialised form is a base64 encoded version struct
   aff4_cipher_data_t (above). Where the nonce is the iv encrypted
   using the key.
*
************************** Methods  **************************/
uint64_t aff4_AES256Password_parse(AES256Password self, char *serialised_form,
				   RDFURN subject);
TDB_DATA *aff4_AES256Password_encode(AES256Password self, RDFURN subject);

uint64_t aff4_AES256Password_decode(AES256Password self, char *data, int length,
				    RDFURN subject);
char *aff4_AES256Password_serialise(AES256Password self, RDFURN subject);

RDFValue aff4_AES256Password_clone(AES256Password self);

uint64_t aff4_AES256Password_encrypt(AES256Password self, int chunk_number,
				     unsigned char *inbuff,
				     unsigned long int inlen,
				     unsigned char *outbuff,
				     unsigned long int length);
uint64_t aff4_AES256Password_decrypt(AES256Password self, int chunk_number,
				     unsigned char *inbuff,
				     unsigned long int inlen,
				     unsigned char *outbuff,
				     unsigned long int length);

/************************** Class AFF4_AFFObject **************************
 All AFF Objects inherit from this one. The URI must be set to
    represent the globally unique URI of this object. 
************************** Methods  **************************/

/*
This is called to set properties on the object
*/
void aff4_AFFObject_set(AFFObject self, char *attribute, RDFValue value);

/*
This is called to set properties on the object
*/
void aff4_AFFObject_add(AFFObject self, char *attribute, RDFValue value);

/*
Finally the object may be ready for use. We return the ready
	 object or NULL if something went wrong.
*/
AFFObject aff4_AFFObject_finish(AFFObject self);

/*
This method is used to return this object to the primary
     resolver cache. The object should not be used after calling this
     as the caller no longer owns it. As far as the caller is
     concerned this is a desctructor and if you need the object again,
     you need to call Resolver.open() to reobtain this.

     In practice this method synchronises the object attributes so
     that a subsequent call to open with a cache miss will be able to
     reconstruct this object exactly as it is now. Once these
     attributes are set, this function calls Resolver.cache_return to
     place the object back in the cache.

     Sometimes this is impossible to do accurately, in which case the
     function can simply choose to free the object and not return it
     to the cache.
*/
void aff4_AFFObject_cache_return(AFFObject self);

/*
When the object is closed it will write itself to its volume.
        This frees the object - do no use it after calling close.
*/
uint64_t aff4_AFFObject_close(AFFObject self);

/************************** Class AFF4_Cache **************************
 The Cache is an object which manages a cache of Object instances
    indexed by a key.

    Each Object can either be in the Cache (in which case it is owned
    by this cache object and will not be freed) or out of the cache
    (in which case it is owned by NULL, and can be freed. When the
    cache gets too full it will start freeing objects it owns.

    Objects are cached with a key reference so this is like a python
    dict or a hash table, except that we can have several objects
    indexed with the same key. If you want to iterate over all the
    objects you need to use the iterator interface.

    NOTE: The cache must be locked if you are using it from multiple
    threads. You can use the Cache->lock mutex or some other mutex
    instead.

    NOTE: After putting the Object in the cache you do not own it -
    and you must not use it (because it might be freed at any time).

************************** Methods  **************************/

/*
These functions can be tuned to manage the hash table. The
        default implementation assumes key is a null terminated
        string.
*/
uint64_t aff4_Cache_hash(Cache self, char *key, int len);

/*
These functions can be tuned to manage the hash table. The
        default implementation assumes key is a null terminated
        string.
*/
uint64_t aff4_Cache_cmp(Cache self, char *other, int len);

/*
Return a cache object or NULL if its not there. The
        object is removed from the cache.
*/
Object aff4_Cache_get(Cache self, char *key, int len);

/*
Returns a reference to the object. The object is still owned
      by the cache. Note that this should only be used in caches which
      do not expire objects or if the cache is locked, otherwise the
      borrowed reference may disappear unexpectadly. References may be
      freed when other items are added.
*/
Object aff4_Cache_borrow(Cache self, char *key, int len);

/*
Store the key, data in a new Cache object. The key and data will be
        stolen.
*/
Cache aff4_Cache_put(Cache self, char *key, int len, Object data);

/*
Returns true if the object is in cache
*/
uint64_t aff4_Cache_present(Cache self, char *key, int len);

/*
This returns an opaque reference to a cache iterator. Note:
        The cache must be locked the entire time between receiving the
        iterator and getting all the objects.
*/
Object aff4_Cache_iter(Cache self, char *key, int len);

/*
This returns an opaque reference to a cache iterator. Note:
        The cache must be locked the entire time between receiving the
        iterator and getting all the objects.
*/
Object aff4_Cache_next(Cache self, Object * iter);

uint64_t aff4_Cache_print_cache(Cache self);

/************************** Class AFF4_ZipFile **************************
 This represents a Zip file 
************************** Methods  **************************/
void aff4_ZipFile_set(ZipFile self, char *attribute, RDFValue value);

void aff4_ZipFile_add(ZipFile self, char *attribute, RDFValue value);

AFFObject aff4_ZipFile_finish(ZipFile self);

void aff4_ZipFile_cache_return(ZipFile self);

uint64_t aff4_ZipFile_close(ZipFile self);

FileLikeObject aff4_ZipFile_open_member(ZipFile self, char *filename, char mode,
					uint16_t compression);
uint64_t aff4_ZipFile_writestr(ZipFile self, char *filename, char *data,
			       int len, uint16_t compression);
uint64_t aff4_ZipFile_load_from(ZipFile self, RDFURN fd_urn, char mode);

/************************** Class AFF4_IntegerArrayInline **************************
 This is used when the array is fairly small and it can fit
         inline 
************************** Methods  **************************/
uint64_t aff4_IntegerArrayInline_parse(IntegerArrayInline self,
				       char *serialised_form, RDFURN subject);
TDB_DATA *aff4_IntegerArrayInline_encode(IntegerArrayInline self,
					 RDFURN subject);
uint64_t aff4_IntegerArrayInline_decode(IntegerArrayInline self, char *data,
					int length, RDFURN subject);
char *aff4_IntegerArrayInline_serialise(IntegerArrayInline self,
					RDFURN subject);
RDFValue aff4_IntegerArrayInline_clone(IntegerArrayInline self);

void aff4_IntegerArrayInline_add(IntegerArrayInline self, unsigned int offset);

/************************** Class AFF4_MapValueInline **************************
 Sometimes its quicker to just serialise the map inline

************************** Methods  **************************/
uint64_t aff4_MapValueInline_parse(MapValueInline self, char *serialised_form,
				   RDFURN subject);
TDB_DATA *aff4_MapValueInline_encode(MapValueInline self, RDFURN subject);

uint64_t aff4_MapValueInline_decode(MapValueInline self, char *data, int length,
				    RDFURN subject);
char *aff4_MapValueInline_serialise(MapValueInline self, RDFURN subject);

RDFValue aff4_MapValueInline_clone(MapValueInline self);

uint64_t aff4_MapValueInline_add_point(MapValueInline self,
				       uint64_t image_offset,
				       uint64_t target_offset, char *target);
RDFURN aff4_MapValueInline_get_range(MapValueInline self, uint64_t readptr,
				     uint64_t * target_offset_at_point,
				     uint64_t * available_to_read,
				     uint32_t * target_idx);

/************************** Class AFF4_Logger **************************
 A logger may be registered with the Resolver. Any objects
         obtained from the Resolver will then use the logger to send
         messages to the user.
     
************************** Methods  **************************/

/*
A logger may be registered with the Resolver. Any objects
         obtained from the Resolver will then use the logger to send
         messages to the user.
     The message sent to the logger is sent from a service (The
     source of the message), at a particular level (how critical it
     is). It talks about a subject (usually the URN of the subject),
     and a message about it.
*/
void aff4_Logger_message(Logger self, int level, char *service, RDFURN subject,
			 char *message);

/************************** Class AFF4_XSDString **************************
 A literal string 
************************** Methods  **************************/
uint64_t aff4_XSDString_parse(XSDString self, char *serialised_form,
			      RDFURN subject);
TDB_DATA *aff4_XSDString_encode(XSDString self, RDFURN subject);

uint64_t aff4_XSDString_decode(XSDString self, char *data, int length,
			       RDFURN subject);
char *aff4_XSDString_serialise(XSDString self, RDFURN subject);

RDFValue aff4_XSDString_clone(XSDString self);

void aff4_XSDString_set(XSDString self, char *string, int length);

char *aff4_XSDString_get(XSDString self);

/************************** Class AFF4_FileBackedObject **************************
 This file like object is backed by a real disk file:

************************** Methods  **************************/
void aff4_FileBackedObject_set(FileBackedObject self, char *attribute,
			       RDFValue value);
void aff4_FileBackedObject_add(FileBackedObject self, char *attribute,
			       RDFValue value);
AFFObject aff4_FileBackedObject_finish(FileBackedObject self);

void aff4_FileBackedObject_cache_return(FileBackedObject self);

uint64_t aff4_FileBackedObject_close(FileBackedObject self);

uint64_t aff4_FileBackedObject_seek(FileBackedObject self, int64_t offset,
				    int whence);
uint64_t aff4_FileBackedObject_read(FileBackedObject self, char *buffer,
				    unsigned long int length);
uint64_t aff4_FileBackedObject_readline(FileBackedObject self, char *buffer,
					unsigned long int length);
uint64_t aff4_FileBackedObject_write(FileBackedObject self, char *buffer,
				     unsigned long int length);
uint64_t aff4_FileBackedObject_tell(FileBackedObject self);

char *aff4_FileBackedObject_get_data(FileBackedObject self);

uint64_t aff4_FileBackedObject_truncate(FileBackedObject self, uint64_t offset);

/************************** Class AFF4_Object **************************
 Base object

************************** Methods  **************************/

/************************** Class AFF4_AES256X509 **************************

   This cipher uses AES256.

   The serialised form is the url of the x509 certificate that was
   used to encrypt the key, with the base64 encoded struct
   aff4_cipher_data_t being encoded as the query string. The security
   manager is used to get both the certificate and the private keys.
*
************************** Methods  **************************/
uint64_t aff4_AES256X509_parse(AES256X509 self, char *serialised_form,
			       RDFURN subject);
TDB_DATA *aff4_AES256X509_encode(AES256X509 self, RDFURN subject);

uint64_t aff4_AES256X509_decode(AES256X509 self, char *data, int length,
				RDFURN subject);
char *aff4_AES256X509_serialise(AES256X509 self, RDFURN subject);

RDFValue aff4_AES256X509_clone(AES256X509 self);

uint64_t aff4_AES256X509_encrypt(AES256X509 self, int chunk_number,
				 unsigned char *inbuff, unsigned long int inlen,
				 unsigned char *outbuff,
				 unsigned long int length);
uint64_t aff4_AES256X509_decrypt(AES256X509 self, int chunk_number,
				 unsigned char *inbuff, unsigned long int inlen,
				 unsigned char *outbuff,
				 unsigned long int length);

/*
This method sets the X509 certificate with which the master
        key is encrypted.
*/
uint64_t aff4_AES256X509_set_authority(AES256X509 self, RDFURN location);

/************************** Class AFF4_RDFParser **************************

************************** Methods  **************************/

/*
Parses data stored in fd using the format specified. fd is assumed
 to contain a base URN specified (or NULL if non specified).
*/
uint64_t aff4_RDFParser_parse(RDFParser self, FileLikeObject fd, char *format,
			      char *base);

/************************** Class AFF4_MapValue **************************

************************** Methods  **************************/
uint64_t aff4_MapValue_parse(MapValue self, char *serialised_form,
			     RDFURN subject);
TDB_DATA *aff4_MapValue_encode(MapValue self, RDFURN subject);

uint64_t aff4_MapValue_decode(MapValue self, char *data, int length,
			      RDFURN subject);
char *aff4_MapValue_serialise(MapValue self, RDFURN subject);

RDFValue aff4_MapValue_clone(MapValue self);

uint64_t aff4_MapValue_add_point(MapValue self, uint64_t image_offset,
				 uint64_t target_offset, char *target);

/*
This function returns information about the current file pointer
     and its view of the target slice.

     DEFAULT(target_idx) = NULL;
*/
RDFURN aff4_MapValue_get_range(MapValue self, uint64_t readptr,
			       uint64_t * target_offset_at_point,
			       uint64_t * available_to_read,
			       uint32_t * target_idx);

/************************** Class AFF4_Resolver **************************
 The resolver is at the heart of the AFF4 specification - it is
    responsible for returning objects keyed by attribute from a
    globally unique identifier (URI) and managing the central
    information store.

************************** Methods  **************************/

/*
This method tries to resolve the provided uri and returns an
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
AFFObject aff4_Resolver_open(Resolver self, RDFURN uri, char mode);

/*
All objects obtained from Resolver.open() need to be
          returned to the cache promptly using this method. NOTE - it
          is an error to be using an object after being returned to
          the cache - it might not be valid and may be gced.

          NOTE for Python bindings - do not use this function, instead
          use AFFObject.cache_return() method which ensures the
          wrapped python object is properly destroyed after this call.
*/
void aff4_Resolver_cache_return(Resolver self, AFFObject obj);

/*
This create a new object of the specified type.

          name specifies the type of object as registered in the type
          handler dispatcher. (e.g. AFF4_ZIP_VOLUME)

          DEFAULT(mode) = "w"
*/
AFFObject aff4_Resolver_create(Resolver self, char *name, char mode);

/*
This function resolves the value in uri and attribute and sets it
          into the RDFValue object. So you must first create such an object
          (e.g. XSDDatetime) and then pass the object here to be updated
          from the data store. Note that only a single value is returned -
          if you want to iterate over all the values for this attribute you
          need to call get_iter and iter_next.
*/
uint64_t aff4_Resolver_resolve_value(Resolver self, RDFURN uri, char *attribute,
				     RDFValue value);

/*
Similar to Resolver.resolve_value, but a new RDFValue is
           allocated with the context provided.
*/
RDFValue aff4_Resolver_resolve_alloc(Resolver self, RDFURN uri,
				     char *attribute);

/*
This is a version of the above which uses an iterator to iterate
          over the list.

          The iterator is obtained using get_iter first. This function
          returns 1 if an iterator can be found (i.e. at least one result
          exists) or 0 if no results exist.

          Each call to iter_next will write a new value into the buffer set
          up by result with maximal length length. Only results matching
          the type specified are returned. We return length written for
          each successful iteration, and zero when we have no more items.
*/
RESOLVER_ITER *aff4_Resolver_get_iter(Resolver self, RDFURN uri,
				      char *attribute);

/*
This method reads the next result from the iterator. result
	  must be an allocated and valid RDFValue object
*/
uint64_t aff4_Resolver_iter_next(Resolver self, RESOLVER_ITER * iter,
				 RDFValue result);

/*
This method is similar to iter_next except the result is
	  allocated to the NULL context. Callers need to talloc_free
	  the result. This advantage of this method is that we dont
	  need to know in advance what type the value is.

          Note - this function advances the iterator.
*/
RDFValue aff4_Resolver_alloc_from_iter(Resolver self, RESOLVER_ITER * iter);

/*
Deletes all values for this attribute from the resolver

          DEFAULT(attribute) = NULL;
*/
void aff4_Resolver_del(Resolver self, RDFURN uri, char *attribute);

/*
Expires this object and all objects it owns.

          NOTE: any outstanding objects will become invalidated. For
          example, if you expire a volume, any outstanding streams
          opened within the volume will become invalidated.
*/
void aff4_Resolver_expire(Resolver self, RDFURN uri);

/*
Sets a new value for an attribute. Note that this function
          clears any previously set values, if you want to create a list
          of values you need to call add_value.

          If iter is given the iterator will be filled into it. The
          iterator can subsequently be used with iter_alloc() to
          reobtain the value.

          DEFAULT(iter) = NULL;
*/
uint64_t aff4_Resolver_set_value(Resolver self, RDFURN uri, char *attribute,
				 RDFValue value, RESOLVER_ITER * iter);

/*
Adds a new value to the value list for this attribute.

          DEFAULT(iter) = NULL;
*/
uint64_t aff4_Resolver_add_value(Resolver self, RDFURN uri, char *attribute,
				 RDFValue value, RESOLVER_ITER * iter);

/*
This function can be used to iterate over all the
           attributes set for a subject. For each attribute, the
           RESOLVER_ITER is set to point to the start of its attribute
           run.

           attribute is set to the value of the attribute which was returned.

           We return 0 if no further attributes are available.
*/
uint64_t aff4_Resolver_attributes_iter(Resolver self, RDFURN urn,
				       XSDString attribute,
				       RESOLVER_ITER * iter);

/*
This returns a unique ID for the given URN. The ID is only
           unique within this resolver.

           DEFAULT(create) = 0
*/
uint64_t aff4_Resolver_get_id_by_urn(Resolver self, RDFURN uri, int create);

/*
This fills the URI specified by id into the uri container
       passed. Returns 1 if the ID is found, or 0 if the ID is not
       found.

       RAISES(func_return == 0, KeyError) = "URI not found for id %llu", (long long unsigned int)id
       *
*/
uint64_t aff4_Resolver_get_urn_by_id(Resolver self, int id, RDFURN uri);

/*
This function is used to register a new RDFValue class with
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
       *
*/
void aff4_Resolver_register_rdf_value_class(Resolver self, RDFValue class_ref);

/*
This is a handler for new types - types get registered here
*/
void aff4_Resolver_register_type_dispatcher(Resolver self, char *type,
					    AFFObject class_ref);
void aff4_Resolver_register_security_provider(Resolver self,
					      SecurityProvider class_ref);

/*
This can be used to install a logger object. All messages
          will then be output through this object.
*/
void aff4_Resolver_register_logger(Resolver self, Logger logger);

/*
A Utility method to create a new instance of a registered
           RDF dataType.
*/
RDFValue aff4_Resolver_new_rdfvalue(Resolver self, char *type);

/*
This function attempts to load the volume stored within the
          FileLikeObject URI provided. If there is a volume, the
          volume URI is set in uri, and we return true.

          We attempt to instantiate all volume drivers in turn until
          one works and then load them from the URI.
*/
uint64_t aff4_Resolver_load(Resolver self, RDFURN uri);

/*
A generic interface to the logger allows any code to send
           messages to the provided logger.
*/
void aff4_Resolver_log(Resolver self, int level, char *service, RDFURN subject,
		       char *messages);

/*
This closes and frees all memory used by the resolver.

           This is generally needed after forking as two resolvers can
           not exist in different processes.
*/
void aff4_Resolver_close(Resolver self);

/*
This is used to flush all our caches
*/
void aff4_Resolver_flush(Resolver self);

/************************** Class AFF4_Graph **************************
 A Graph is a named collection of RDF statements about various
    objects.

    When the graph is stored it will serialise into a segment with all
    the statements contained with it using the appropriate RDF
    serialization.

************************** Methods  **************************/
void aff4_Graph_set(Graph self, char *attribute, RDFValue value);

void aff4_Graph_add(Graph self, char *attribute, RDFValue value);

AFFObject aff4_Graph_finish(Graph self);

void aff4_Graph_cache_return(Graph self);

uint64_t aff4_Graph_close(Graph self);

/*
Add a new triple to this Graph
*/
void aff4_Graph_set_triple(Graph self, RDFURN subject, char *attribute,
			   RDFValue value);

/************************** Class AFF4_AFF4Cipher **************************
 This is the abstract cipher class - all ciphers must implement
    these methods.

************************** Methods  **************************/
uint64_t aff4_AFF4Cipher_parse(AFF4Cipher self, char *serialised_form,
			       RDFURN subject);
TDB_DATA *aff4_AFF4Cipher_encode(AFF4Cipher self, RDFURN subject);

uint64_t aff4_AFF4Cipher_decode(AFF4Cipher self, char *data, int length,
				RDFURN subject);
char *aff4_AFF4Cipher_serialise(AFF4Cipher self, RDFURN subject);

RDFValue aff4_AFF4Cipher_clone(AFF4Cipher self);

uint64_t aff4_AFF4Cipher_encrypt(AFF4Cipher self, int chunk_number,
				 unsigned char *inbuff, unsigned long int inlen,
				 unsigned char *outbuff,
				 unsigned long int length);
uint64_t aff4_AFF4Cipher_decrypt(AFF4Cipher self, int chunk_number,
				 unsigned char *inbuff, unsigned long int inlen,
				 unsigned char *outbuff,
				 unsigned long int length);

/************************** Class AFF4_SecurityProvider **************************
 This class implements a security provider. This is used by AFF4
    ciphers to request passwords or certificates. Users of the library
    should implement their own security provider and install it using
    the Resolver.install_security_provider() function. When a cipher
    needs to obtain a passphrase or certificate they call here to
    obtain it - the user code can choose how to manage their own
    password (e.g. bring up an interactive GUI, use the OS secure
    storage facility etc).

    There is currently only a single global security provider.

************************** Methods  **************************/

/*
This is called when we need to obtain a passphrase for a subject
        stream. Returns 0 on error, and the length of the passphrase in
        len (which must initially contain the length of the passphrase
        buffer).
*/
char *aff4_SecurityProvider_passphrase(SecurityProvider self, char *cipher_type,
				       RDFURN subject);

/*
This method should return the location (URL) of the
        certificate that will be used to encrypt the data.
*/
char *aff4_SecurityProvider_x509_private_key(SecurityProvider self,
					     char *cert_name, RDFURN subject);

/************************** Class AFF4_Key **************************

************************** Methods  **************************/

/************************** Class AFF4_XSDDatetime **************************
 Dates serialised according the XML standard 
************************** Methods  **************************/
uint64_t aff4_XSDDatetime_parse(XSDDatetime self, char *serialised_form,
				RDFURN subject);
TDB_DATA *aff4_XSDDatetime_encode(XSDDatetime self, RDFURN subject);

uint64_t aff4_XSDDatetime_decode(XSDDatetime self, char *data, int length,
				 RDFURN subject);
char *aff4_XSDDatetime_serialise(XSDDatetime self, RDFURN subject);

RDFValue aff4_XSDDatetime_clone(XSDDatetime self);

void aff4_XSDDatetime_set(XSDDatetime self, struct timeval time);

/************************** Class AFF4_FileLikeObject **************************

************************** Methods  **************************/
void aff4_FileLikeObject_set(FileLikeObject self, char *attribute,
			     RDFValue value);
void aff4_FileLikeObject_add(FileLikeObject self, char *attribute,
			     RDFValue value);
AFFObject aff4_FileLikeObject_finish(FileLikeObject self);

void aff4_FileLikeObject_cache_return(FileLikeObject self);

uint64_t aff4_FileLikeObject_close(FileLikeObject self);

/*
Seek the file like object to the specified offset.

     DEFAULT(whence) = 0
*/
uint64_t aff4_FileLikeObject_seek(FileLikeObject self, int64_t offset,
				  int whence);

/*
Seek the file like object to the specified offset.

     DEFAULT(whence) = 0
*/
uint64_t aff4_FileLikeObject_read(FileLikeObject self, char *buffer,
				  unsigned long int length);

/*
A variant of read above that will read upto the next \r or
        \r\n.

        DEFAULT(length) = 1024
*/
uint64_t aff4_FileLikeObject_readline(FileLikeObject self, char *buffer,
				      unsigned long int length);

/*
A variant of read above that will read upto the next \r or
        \r\n.

        DEFAULT(length) = 1024
*/
uint64_t aff4_FileLikeObject_write(FileLikeObject self, char *buffer,
				   unsigned long int length);

/*
A variant of read above that will read upto the next \r or
        \r\n.

        DEFAULT(length) = 1024
*/
uint64_t aff4_FileLikeObject_tell(FileLikeObject self);

/*
This can be used to get the content of the FileLikeObject in a
 big buffer of data. The data will be cached with the
 FileLikeObject. Its only really suitable for smallish amounts of
 data - and checks to ensure that file size is less than MAX_CACHED_FILESIZE
*/
char *aff4_FileLikeObject_get_data(FileLikeObject self);

/*
This method is just like the standard ftruncate call
*/
uint64_t aff4_FileLikeObject_truncate(FileLikeObject self, uint64_t offset);

/************************** Class AFF4_RDFSerializer **************************
*** Following is an implementation of a serialiser 
************************** Methods  **************************/
uint64_t aff4_RDFSerializer_serialize_urn(RDFSerializer self, RDFURN urn);

uint64_t aff4_RDFSerializer_serialize_statement(RDFSerializer self,
						RESOLVER_ITER * iter,
						RDFURN urn, RDFURN attribute);
void aff4_RDFSerializer_set_namespace(RDFSerializer self, char *prefix,
				      char *namespace);
void aff4_RDFSerializer_close(RDFSerializer self);

/************************** Class AFF4_ZipFileStream **************************
 This is a FileLikeObject which is used to provide access to zip
 archive members. Currently only accessible through
 ZipFile.open_member()

************************** Methods  **************************/
void aff4_ZipFileStream_set(ZipFileStream self, char *attribute,
			    RDFValue value);
void aff4_ZipFileStream_add(ZipFileStream self, char *attribute,
			    RDFValue value);
AFFObject aff4_ZipFileStream_finish(ZipFileStream self);

void aff4_ZipFileStream_cache_return(ZipFileStream self);

uint64_t aff4_ZipFileStream_close(ZipFileStream self);

uint64_t aff4_ZipFileStream_seek(ZipFileStream self, int64_t offset,
				 int whence);
uint64_t aff4_ZipFileStream_read(ZipFileStream self, char *buffer,
				 unsigned long int length);
uint64_t aff4_ZipFileStream_readline(ZipFileStream self, char *buffer,
				     unsigned long int length);
uint64_t aff4_ZipFileStream_write(ZipFileStream self, char *buffer,
				  unsigned long int length);
uint64_t aff4_ZipFileStream_tell(ZipFileStream self);

char *aff4_ZipFileStream_get_data(ZipFileStream self);

uint64_t aff4_ZipFileStream_truncate(ZipFileStream self, uint64_t offset);

/************************** Class AFF4_IntegerArrayBinary **************************
 An integer array stores an array of integers
         efficiently. This variant stores it in a binary file.
     
************************** Methods  **************************/
uint64_t aff4_IntegerArrayBinary_parse(IntegerArrayBinary self,
				       char *serialised_form, RDFURN subject);
TDB_DATA *aff4_IntegerArrayBinary_encode(IntegerArrayBinary self,
					 RDFURN subject);
uint64_t aff4_IntegerArrayBinary_decode(IntegerArrayBinary self, char *data,
					int length, RDFURN subject);
char *aff4_IntegerArrayBinary_serialise(IntegerArrayBinary self,
					RDFURN subject);
RDFValue aff4_IntegerArrayBinary_clone(IntegerArrayBinary self);

/*
Add a new integer to the array
*/
void aff4_IntegerArrayBinary_add(IntegerArrayBinary self, unsigned int offset);

/************************** Class AFF4_TDB **************************
This is just a wrapper around tdb so it can easily be used
          from python etc. 
************************** Methods  **************************/

/*
This is the constructor for a new TDB database.

         DEFAULT(mode) = 0;
*/
void aff4_TDB_store(TDB self, char *key, int key_len, char *data, int len);

/*
This is the constructor for a new TDB database.

         DEFAULT(mode) = 0;
*/
TDB_DATA aff4_TDB_fetch(TDB self, char *key, int len);

/************************** Class AFF4_Image **************************
 The Image Stream represents an Image in chunks 
************************** Methods  **************************/
void aff4_Image_set(Image self, char *attribute, RDFValue value);

void aff4_Image_add(Image self, char *attribute, RDFValue value);

AFFObject aff4_Image_finish(Image self);

void aff4_Image_cache_return(Image self);

uint64_t aff4_Image_close(Image self);

uint64_t aff4_Image_seek(Image self, int64_t offset, int whence);

uint64_t aff4_Image_read(Image self, char *buffer, unsigned long int length);

uint64_t aff4_Image_readline(Image self, char *buffer,
			     unsigned long int length);
uint64_t aff4_Image_write(Image self, char *buffer, unsigned long int length);

uint64_t aff4_Image_tell(Image self);

char *aff4_Image_get_data(Image self);

uint64_t aff4_Image_truncate(Image self, uint64_t offset);

/*
This sets the number of working threads.

      The default number of threads is zero (no threads). Set to a
      higher number to utilize multiple threads here.
*/
void aff4_Image_set_workers(Image self, int workers);

/************************** Class AFF4_Encrypted **************************

   Encryption is handled via two components, the encrypted stream and
   the cipher. An encrypted stream is an FileLikeObject with the
   following AFF4 attributes:

   AFF4_CHUNK_SIZE = Data will be broken into these chunks and
                     encrypted independantly.

   AFF4_STORED     = Data will be stored on this backing stream.

   AFF4_CIPHER     = This is an RDFValue which extends the AFF4Cipher
                     class. More on that below.

   When opened, the FileLikeObject is created by instantiating a
   cipher from the AFF4_CIPHER attribute. Data is then divided into
   chunks and each chunk is encrypted using the cipher, and then
   written to the backing stream.

   A cipher is a class which extends the AFF4Cipher baseclass.

   Of course a valid cipher must also implement valid serialization
   methods and also some way of key initialization. Chunks must be
   whole multiples of blocksize.

************************** Methods  **************************/
void aff4_Encrypted_set(Encrypted self, char *attribute, RDFValue value);

void aff4_Encrypted_add(Encrypted self, char *attribute, RDFValue value);

AFFObject aff4_Encrypted_finish(Encrypted self);

void aff4_Encrypted_cache_return(Encrypted self);

uint64_t aff4_Encrypted_close(Encrypted self);

uint64_t aff4_Encrypted_seek(Encrypted self, int64_t offset, int whence);

uint64_t aff4_Encrypted_read(Encrypted self, char *buffer,
			     unsigned long int length);
uint64_t aff4_Encrypted_readline(Encrypted self, char *buffer,
				 unsigned long int length);
uint64_t aff4_Encrypted_write(Encrypted self, char *buffer,
			      unsigned long int length);
uint64_t aff4_Encrypted_tell(Encrypted self);

char *aff4_Encrypted_get_data(Encrypted self);

uint64_t aff4_Encrypted_truncate(Encrypted self, uint64_t offset);

/************************** Class AFF4_URLParse **************************
** A class used to parse URNs 
************************** Methods  **************************/

/*
Parses the url given and sets internal attributes.
*/
uint64_t aff4_URLParse_parse(URLParse self, char *url);

/*
Returns the internal attributes joined together into a valid
         URL.
*/
char *aff4_URLParse_string(URLParse self);

/************************** Class AFF4_RDFValue **************************
*** The RDF resolver stores objects inherited from this one.

       You can define other objects and register them using the
       register_rdf_value_class() function. You will need to extend
       this base class and initialise it with a unique dataType URN.

       RDFValue is the base class for all other values.
*****
************************** Methods  **************************/

/*
This method is called to parse a serialised form into this
	 instance. Return 1 if parsing is successful, 0 if error
	 occured.

         DEFAULT(subject) = NULL;
*/
uint64_t aff4_RDFValue_parse(RDFValue self, char *serialised_form,
			     RDFURN subject);

/*
This method is called to serialise this object into the
	 TDB_DATA struct for storage in the TDB data store. The new
	 memory will be allocated with this object's context and must
	 be freed by the caller.
*/
TDB_DATA *aff4_RDFValue_encode(RDFValue self, RDFURN subject);

/*
This method is used to decode this object from the
         data_store. The fd is seeked to the start of this record.
*/
uint64_t aff4_RDFValue_decode(RDFValue self, char *data, int length,
			      RDFURN subject);

/*
This method will serialise the value into a null terminated
	  string for export into RDF. The returned string will be
	  allocated to the NULL context and should be unlinked by the caller.

          DEFAULT(subject) = NULL;
*/
char *aff4_RDFValue_serialise(RDFValue self, RDFURN subject);

/*
Makes a copy of this value - the copy should be made as
          efficiently as possible (It might just take a reference to
          memory instead of copying it).
*/
RDFValue aff4_RDFValue_clone(RDFValue self);

/************************** Class AFF4_AFF4Volume **************************
 This is an abstract class that implements AFF4 volumes 
************************** Methods  **************************/
void aff4_AFF4Volume_set(AFF4Volume self, char *attribute, RDFValue value);

void aff4_AFF4Volume_add(AFF4Volume self, char *attribute, RDFValue value);

AFFObject aff4_AFF4Volume_finish(AFF4Volume self);

void aff4_AFF4Volume_cache_return(AFF4Volume self);

uint64_t aff4_AFF4Volume_close(AFF4Volume self);

/*
This is an abstract class that implements AFF4 volumes  This method opens an existing member or creates a new one. We
 return a file like object which may be used to read and write the
 member. If we open a member for writing the zip file will be locked
 (so another attempt to open a new member for writing will raise,
 until this member is promptly closed). The ZipFile must have been
 called with create_new_volume or append_volume before.
 DEFAULT(mode) = "r";
 DEFAULT(compression) = ZIP_DEFLATE;
*/
FileLikeObject aff4_AFF4Volume_open_member(AFF4Volume self, char *filename,
					   char mode, uint16_t compression);

/*
A convenience function for storing a string as a new file (it
 basically calls open_member, writes the string then closes it).
 DEFAULT(compression) = ZIP_DEFLATE
*/
uint64_t aff4_AFF4Volume_writestr(AFF4Volume self, char *filename, char *data,
				  int len, uint16_t compression);

/*
Load an AFF4 volume from the URN specified. We parse all the RDF
     serializations.

     DEFAULT(mode) = "r"
*/
uint64_t aff4_AFF4Volume_load_from(AFF4Volume self, RDFURN fd_urn, char mode);

/************************** Class AFF4_XSDInteger **************************
 An integer encoded according the XML RFC. 
************************** Methods  **************************/
uint64_t aff4_XSDInteger_parse(XSDInteger self, char *serialised_form,
			       RDFURN subject);
TDB_DATA *aff4_XSDInteger_encode(XSDInteger self, RDFURN subject);

uint64_t aff4_XSDInteger_decode(XSDInteger self, char *data, int length,
				RDFURN subject);
char *aff4_XSDInteger_serialise(XSDInteger self, RDFURN subject);

RDFValue aff4_XSDInteger_clone(XSDInteger self);

void aff4_XSDInteger_set(XSDInteger self, uint64_t value);

uint64_t aff4_XSDInteger_get(XSDInteger self);

/************************** Class AFF4_RDFURN **************************
 A URN for use in the rest of the library 
************************** Methods  **************************/
uint64_t aff4_RDFURN_parse(RDFURN self, char *serialised_form, RDFURN subject);

TDB_DATA *aff4_RDFURN_encode(RDFURN self, RDFURN subject);

uint64_t aff4_RDFURN_decode(RDFURN self, char *data, int length,
			    RDFURN subject);
char *aff4_RDFURN_serialise(RDFURN self, RDFURN subject);

RDFValue aff4_RDFURN_clone(RDFURN self);

void aff4_RDFURN_set(RDFURN self, char *urn);

/*
Make a new RDFURN as a copy of this one
*/
RDFURN aff4_RDFURN_copy(RDFURN self);

/*
Add a relative stem to the current value. If urn is a fully
        qualified URN, we replace the current value with it.
*/
void aff4_RDFURN_add(RDFURN self, char *urn);

/*
This adds the binary string in filename into the end of the
     URL query string, escaping invalid characters.
*/
void aff4_RDFURN_add_query(RDFURN self, unsigned char *query, unsigned int len);

/*
This method returns the relative name
*/
TDB_DATA aff4_RDFURN_relative_name(RDFURN self, RDFURN volume);
