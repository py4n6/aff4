#ifndef __ZIP_H
#define __ZIP_H

#include "misc.h"
#include "class.h"
#include "talloc.h"
#include "list.h"
#include <zlib.h>
#include "fif.h"

#define HASH_TABLE_SIZE 256
#define CACHE_SIZE 5

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

/** As we parse these fields we populate the oracle */
struct CDFileHeader {
  uint32_t magic;
  uint16_t version_made_by;
  uint16_t version_needed;
  uint16_t flags;
  uint16_t compression_method;	/* aff2volatile:compression */
  uint16_t dostime;		/* aff2volatile:timestamp */
  uint16_t dosdate;
  uint32_t crc32;
  uint32_t compress_size;	/* aff2volatile:compress_size */
  uint32_t file_size;		/* aff2volatile:file_size */
  uint16_t file_name_length;
  uint16_t extra_field_len;
  uint16_t file_comment_length;
  uint16_t disk_number_start;
  uint16_t internal_file_attr;
  uint32_t external_file_attr;
  uint32_t relative_offset_local_header; /* aff2volatile:header_offset */
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

/** This represents a Zip file */
CLASS(ZipFile, AFFObject)
     // This keeps the end of central directory struct so we can
     // recopy it when we update the CD.
     struct EndCentralDirectory *end;

     // This is our own current URN - new files will be appended to
     // that
     char *parent_urn;

     /** A zip file is opened on a file like object */
     ZipFile METHOD(ZipFile, Con, char *file_urn);

// Fetch a member as a string - this is suitable for small memebrs
// only as we allocate memory for it. The buffer callers receive will
// owned by ctx. 
     char *METHOD(ZipFile, read_member, void *ctx,
		  char *filename, int *len);

// This method is called to specify a new volume for us to use. If we
// already have an existing volume, we will automatically call close
// on it. The volume parent_urn must already exist (it can be newly
// created).
     int METHOD(ZipFile, create_new_volume, char *parent_urn);

// This method opens an existing member or creates a new one. We
// return a file like object which may be used to read and write the
// member. If we open a member for writing the zip file will be locked
// (so another attempt to open a new member for writing will raise,
// until this member is promptly closed). The ZipFile must have been
// called with create_new_volume or append_volume before.
     FileLikeObject METHOD(ZipFile, open_member, char *filename, char mode,
			   char *extra, uint16_t extra_field_len,
			   int compression);

// This method flushes the central directory and finalises the
// file. The file may still be accessed for reading after this.
     void METHOD(ZipFile, close);

// A convenience function for storing a string as a new file (it
// basically calls open_member, writes the string then closes it).
     void METHOD(ZipFile, writestr, char *filename, char *data, int len,
		 char *extra, int extra_field_len,
		 int compression);
END_CLASS

#define ZIP_STORED 0
#define ZIP_DEFLATE 8

// This is a FileLikeObject which is used from within the Zip file:
CLASS(ZipFileStream, FileLikeObject)
     z_stream strm;
     uint64_t file_offset;
     char *container_urn;
     char *parent_urn;
     uint32_t crc32;
     uint32_t compress_size;
     uint32_t compression;
     char mode;

// This is the constructor for the file like object. Note that we
// steal the underlying file pointer which should be the underlying
// zip file and should be given to us already seeked to the right
// place.
     ZipFileStream METHOD(ZipFileStream, Con, char *filename, 
			  char *parent_urn, char *container_urn,
			  char mode);
END_CLASS

// This is the main FIF class - it manages the Zip archive
CLASS(FIFFile, ZipFile)
END_CLASS

#endif
