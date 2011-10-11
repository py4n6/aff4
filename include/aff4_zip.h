#ifndef __ZIP_H
#define __ZIP_H

/* This is the largest file size which may be represented by a regular
   zip file without using Zip64 extensions. (It is a constant but not
   a macro so we can test it).
*/
static uint64_t ZIP64_LIMIT=((1LL << 31)-1);


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
  uint16_t compression_method;  /* aff2volatile:compression */
  uint16_t dostime;             /* aff2volatile:timestamp */
  uint16_t dosdate;
  uint32_t crc32;
  uint32_t compress_size;       /* aff2volatile:compress_size */
  uint32_t file_size;           /* aff2volatile:file_size */
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
}__attribute__((packed));


struct Zip64CDLocator {
  uint32_t magic;
  uint32_t disk_with_cd;
  uint64_t offset_of_end_cd;
  uint32_t number_of_disks;
}__attribute__((packed));


struct ZipFile_t;


/** Reresents a single file in the archive. */
CLASS(ZipSegment, FileLikeObject)
  struct list_head members;
  struct CDFileHeader cd;

  /* These need to be configured before calling finish() */
  RDFURN container;
  int compression_method;
  time_t timestamp;

  z_stream strm;
  uint64_t offset_of_file_header;
  XSDString filename;

  /* Data is compressed to this buffer and only written when the Segment is
   * closed.
   *
   * Note that in AFF4 we assume segments are not too large so we can cache them
   *  in memory.
   */
  StringIO buffer;
END_CLASS


/** This represents a Zip file */
CLASS(ZipFile, AFF4Volume)
     // This keeps the end of central directory struct so we can
     // recopy it when we update the CD.
     struct EndCentralDirectory *end;

     // All our members
     struct list_head members;

     /** Some commonly used RDF types */
     XSDInteger directory_offset;
     RDFURN storage_urn;
     int compression_method;

    /* The file we are stored on. */
    FileLikeObject backing_store;
END_CLASS

#define ZIP_STORED 0
#define ZIP_DEFLATE 8
#endif   /* __ZIP_H */
