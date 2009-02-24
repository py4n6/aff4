#include "misc.h"
#include "class.h"
#include "talloc.h"
#include "list.h"
#include <zlib.h>

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
  char *filename;
  char *extra_field;
  char *file_comment;
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

// Base class for file like objects
CLASS(FileLikeObject, Object)
     int64_t readptr;
     uint64_t size;
     char *name;
     
     uint64_t METHOD(FileLikeObject, seek, int64_t offset, int whence);
     int METHOD(FileLikeObject, read, char *buffer, int length);
     int METHOD(FileLikeObject, write, char *buffer, int length);
     uint64_t METHOD(FileLikeObject, tell);
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
     // ZipInfo items can form a list for the CD
     struct list_head list;

     struct CDFileHeader cd_header;
     // Each ZipInfo object remembers which file it came from:
     FileLikeObject fd;

     // ZipInfo items may hold a cache of the decompressed data - they
     // belong to the cache list and may hold the data
     struct list_head cache_list;
     char *cached_data;

     ZipInfo METHOD(ZipInfo, Con, FileLikeObject fd);
END_CLASS

/** This represents a Zip file */
CLASS(ZipFile, Object)
/* We maintain read and write pointers separately to allow the file to
   be updated at the same time as its read */
     uint64_t writeptr;
     uint64_t readptr;
     uint64_t directory_offset;
     ZipInfo CentralDirectory;
     FileLikeObject writer;
     FileLikeObject fd;
     struct EndCentralDirectory *end;

     /** This is a hash table of ZipInfo objects - makes for quick
	 access based on the filename.
     */
     ZipInfo *hash_table;

     /** This is a chunk cache. Its a sorted list in order of usage
	 (newer pages to the front) 
     */
     int cache_size;
     int max_cache_size;
     ZipInfo cache_head;

     /** A zip file is opened on a file like object */
     ZipFile METHOD(ZipFile, Con, FileLikeObject file);

// Fetch a member as a string - this is suitable for small memebrs
// only as we allocate memory for it.
     int METHOD(ZipFile, read_member, char *filename, char **buffer, int *len);

// This method is called to create a new volume on the FileLikeObject
     void METHOD(ZipFile, create_new_volume, FileLikeObject file);

// This method opens an existing member or creates a new one. We
// return a file like object which may be used to read and write the
// member. If we open a member for writing the zip file will be locked
// (so another attempt to open a new member for writing will raise,
// until this member is promptly closed).
     FileLikeObject METHOD(ZipFile, open_member, char *filename, char mode,
			   int compression);

// This method flushes the central directory and finalises the
// file. The file may still be accessed for reading after this.
     void METHOD(ZipFile, close);

// A convenience function for storing a string as a new file
     void METHOD(ZipFile, writestr, char *filename, char *data, int len, int compression);

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
