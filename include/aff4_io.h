/*
** aff4_io.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Thu Nov 12 20:38:45 2009 mic
** Last update Tue Feb  2 16:24:59 2010 mic
*/

#ifndef        AFF4_IO_H_
# define       AFF4_IO_H_

#include "aff4_rdf.h"


/** All AFF Objects inherit from this one. The URI must be set to
    represent the globally unique URI of this object. */
CLASS(AFFObject, Object)
     // The resolver we should use.
     struct Resolver_t *resolver;
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
      AFFObject METHOD(AFFObject, Con, RDFURN urn, char mode, \
                       struct Resolver_t *resolver);

     /** This is called to set properties on the object */
     void METHOD(AFFObject, set, char *attribute, RDFValue value);
     void METHOD(AFFObject, add, char *attribute, RDFValue value);

     /** Get a list of attributes from the object. */
     RDFValue METHOD(AFFObject, resolve, void *ctx, char *attribute);

     /** Finally the object may be ready for use. We return the ready
	 object or NULL if something went wrong.
     */
     int METHOD(AFFObject, finish);

     /* When the object is closed it will write itself to its volume.
        This frees the object - do no use it after calling close.
      */
     DESTRUCTOR int METHOD(AFFObject, close);


     /*  This method is used to delete the object from the resolver.
         It should also call the delete method for all the objects
         contained within this one or that can be inferred to be
         incorrect.

         This method is primarily used to ensure that when
         inconsistant information is found about the world, the
         resolver information is invalidated.
     */
     void CLASS_METHOD(delete_urn, RDFURN urn);

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

PROXY_CLASS(AFFObject);


// Base class for file like objects
#define MAX_CACHED_FILESIZE 1e6

CLASS(FileLikeObject, AFFObject)
     int64_t readptr;

     /** Seek the file like object to the specified offset.

     DEFAULT(whence) = 0
     */
     uint64_t METHOD(FileLikeObject, seek, int64_t offset, int whence);
     int METHOD(FileLikeObject, read, OUT char *buffer, \
                unsigned int length);

     /* A variant of read above that will read upto the next \r or
        \r\n.

        DEFAULT(length) = 1024
     */
     int METHOD(FileLikeObject, readline, OUT char *buffer, \
                unsigned int length);

     int METHOD(FileLikeObject, write, char *buffer, \
                unsigned int length);

     uint64_t METHOD(FileLikeObject, tell);

  // This can be used to get the content of the FileLikeObject in a
  // big buffer of data. The data will be cached with the
  // FileLikeObject. Its only really suitable for smallish amounts of
  // data - and checks to ensure that file size is less than MAX_CACHED_FILESIZE
     XSDString METHOD(FileLikeObject, get_data, void *ctx);

  // This method is just like the standard ftruncate call
     int METHOD(FileLikeObject, truncate, uint64_t offset);

// This closes the FileLikeObject and also frees it - it is not valid
// to use the FileLikeObject after calling this (it gets free'd).
//     DESTRUCTOR int METHOD(FileLikeObject, close);
END_CLASS

// This file like object is backed by a real disk file:
CLASS(FileBackedObject, FileLikeObject)
     int fd;
END_CLASS

PROXY_CLASS(FileLikeObject);

     /** This is an abstract class that implements AFF4 volumes */
CLASS(AFF4Volume, AFFObject)
// This method opens an existing member or creates a new one. We
// return a file like object which may be used to read and write the
// member. If we open a member for writing the zip file will be locked
// (so another attempt to open a new member for writing will raise,
// until this member is promptly closed). The ZipFile must have been
// called with create_new_volume or append_volume before.
//
// DEFAULT(mode) = "r";
// DEFAULT(compression) = ZIP_DEFLATE;
     FileLikeObject METHOD(AFF4Volume, open_member, RDFURN filename, \
                           char mode,  int compression);

// This method flushes the central directory and finalises the
// file. The file may still be accessed for reading after this.
//     DESTRUCTOR int METHOD(AFF4Volume, close);

// A convenience function for storing a string as a new file (it
// basically calls open_member, writes the string then closes it).
//
// DEFAULT(compression) = ZIP_DEFLATE
     int METHOD(AFF4Volume, writestr, char *filename, char *data, \
                int len, uint16_t compression);

  /* Load an AFF4 volume from the URN specified. We parse all the RDF
     serializations.

     DEFAULT(mode) = "r"
  */
     int METHOD(AFF4Volume, load_from, RDFURN fd_urn, char mode);
END_CLASS

#endif      /* !AFF4_IO_H_ */
