#include "misc.h"
#include "class.h"
#include "talloc.h"
#include "list.h"
#include <zlib.h>

#define HASH_TABLE_SIZE 256
#define CACHE_SIZE 0

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
}__attribute__((packed));

struct CDFileHeader {
  uint32_t magic;
  uint16_t version_made_by;
  uint16_t version_needed;
  uint16_t flags;
  uint16_t compression_method;
  uint16_t dostime;
  uint16_t dosdate;
  uint32_t crc32;
  uint32_t compress_size;
  uint32_t file_size;
  uint16_t file_name_length;
  uint16_t extra_field_len;
  uint16_t file_comment_length;
  uint16_t disk_number_start;
  uint16_t internal_file_attr;
  uint32_t external_file_attr;
  uint32_t relative_offset_local_header;
}__attribute__((packed));

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
}__attribute__((packed));


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

// Store the key, data in a new Cache object. The key and data will be
// stolen.
     void METHOD(Cache, put, void *key, void *data, int data_len);
END_CLASS

// Base class for file like objects
CLASS(FileLikeObject, Object)
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

     FileBackedObject METHOD(FileBackedObject, con, char *filename, char mode);
END_CLASS

/** This represents a single Zip entry.  We only implement as much as
    we need for FIF.
*/
CLASS(ZipInfo, Object)
     struct CDFileHeader cd_header;

     // These maintain some information about the archive memeber
     char *filename;
     // This is an extra piece of data associated with the file
     // header.
     char *extra_field;

     // This extra piece of data is associated with the Central
     // Directory (Currently not used in this implementation)
     char *file_comment;

     // Each ZipInfo object remembers which file it came from:
     FileLikeObject fd;

     // A shortcut way of going to the start of the file contents
     // (i.e. points to the relative_offset_local_header + sizeof(file
     // header). This helps because file header varies in size.
     uint64_t file_offset;

     ZipInfo METHOD(ZipInfo, Con, FileLikeObject fd);
END_CLASS

/** This represents a Zip file */
CLASS(ZipFile, Object)
// This is the end of the last file record and the start of the
// central directory. When adding a new file to the archive we just go
// there.
     uint64_t directory_offset;
     FileLikeObject writer;
     FileLikeObject fd;
     // This keeps the end of central directory struct so we can
     // recopy it when we update the CD.
     struct EndCentralDirectory *end;

     // A Cache of ZipInfo objects. These objects represent the
     // archive members in the CD. Note that they do not expire - we
     // must keep them all alive so we can access any member of the
     // volume set at once.
     Cache zipinfo_cache;

     // This is a cache of file data saves us decompressing the same
     // file too many times.
     Cache file_data_cache;

     // This is a cache for extra fields
     Cache extra_cache;

     /** This flag is set when the ZipFile is modified and a new CD
	 needs to be rewritten on close
     **/
     int _didModify;

     /** A zip file is opened on a file like object */
     ZipFile METHOD(ZipFile, Con, FileLikeObject file);

// Fetch a member as a string - this is suitable for small memebrs
// only as we allocate memory for it. The buffer callers receive will
// not be owned by the callers. Callers should never free it nor steal
// it they must copy it out if they want to keep it.
     char *METHOD(ZipFile, read_member, char *filename, int *len);

// This method is called to create a new volume on the
// FileLikeObject. This call must preceed any calls to open_member for
// writing.
     void METHOD(ZipFile, create_new_volume, FileLikeObject file);

// This method opens an existing member or creates a new one. We
// return a file like object which may be used to read and write the
// member. If we open a member for writing the zip file will be locked
// (so another attempt to open a new member for writing will raise,
// until this member is promptly closed). The ZipFile must have been
// called with create_new_volume or append_volume before.
     FileLikeObject METHOD(ZipFile, open_member, char *filename, char mode,
			   char *extra, int extra_field_len,
			   int compression);

// This method flushes the central directory and finalises the
// file. The file may still be accessed for reading after this.
     void METHOD(ZipFile, close);

// A convenience function for storing a string as a new file (it
// basically calls open_member, writes the string then closes it).
     void METHOD(ZipFile, writestr, char *filename, char *data, int len,
		 char *extra, int extra_field_len,
		 int compression);

/* A method to fetch the ZipInfo corresponding with a particular
   filename within the archive */
     ZipInfo METHOD(ZipFile, fetch_ZipInfo, char *filename);

END_CLASS

#define ZIP_STORED 0
#define ZIP_DEFLATE 8

// This is a FileLikeObject which is used from within the Zip file:
CLASS(ZipFileStream, FileLikeObject)
     ZipFile zip;
     z_stream strm;
     uint64_t zip_offset;
     char mode;
     ZipInfo zinfo;

// This is the constructor for the file like object. Note that we
// steal the underlying file pointer which should be the underlying
// zip file and should be given to us already seeked to the right
// place.
     ZipFileStream METHOD(ZipFileStream, Con, ZipFile zip,
			  FileLikeObject fd, char *filename,
			  char mode, int compression, 
			  uint64_t header_offset);
END_CLASS
