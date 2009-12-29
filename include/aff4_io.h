/*
** aff4_io.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Thu Nov 12 20:38:45 2009 mic
** Last update Sat Dec 26 23:34:15 2009 mic
*/

#ifndef   	AFF4_IO_H_
# define   	AFF4_IO_H_

#include "aff4_rdf.h"

struct RDFValue;

/** All AFF Objects inherit from this one. The URI must be set to
    represent the globally unique URI of this object. */
CLASS(AFFObject, Object)
     RDFURN urn;

     // Is this object a reader or a writer?
     char mode;

     /** Any object may be asked to be constructed from its URI */
     AFFObject METHOD(AFFObject, Con, RDFURN urn, char mode);

     /** This is called to set properties on the object */
     void METHOD(AFFObject, set_property, char *attribute, RDFValue value);

     /** Finally the object may be ready for use. We return the ready
	 object or NULL if something went wrong.
     */
     DESTRUCTOR AFFObject METHOD(AFFObject, finish);

     /** This method is used to return this object to the primary
     resolver cache. The object should not be used after calling this
     as the caller no longer owns it. As far as the caller is
     concerned this is a desctructor and if you need the object again,
     you need to call Resolver.open() to reobtain this.
     */
     DESTRUCTOR void METHOD(AFFObject, cache_return);

     /*  This method is used to delete the object from the resolver.
         It should also call the delete method for all the objects
         contained within this one or that can be inferred to be
         incorrect.

         This method is primarily used to ensure that when
         inconsistant information is found about the world, the
         resolver information is invalidated.
     */
     void CLASS_METHOD(delete, RDFURN urn);

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
#define MAX_CACHED_FILESIZE 1e6

CLASS(FileLikeObject, AFFObject)
     int64_t readptr;
     XSDInteger size;
     char mode;
     char *data;

     /** Seek the file like object to the specified offset.

     DEFAULT(whence) = 0
     */
     uint64_t METHOD(FileLikeObject, seek, int64_t offset, int whence);
     int METHOD(FileLikeObject, read, OUT char *buffer, unsigned long int length);
     int METHOD(FileLikeObject, write, char *buffer, unsigned long int length);
     uint64_t METHOD(FileLikeObject, tell);

  // This can be used to get the content of the FileLikeObject in a
  // big buffer of data. The data will be cached with the
  // FileLikeObject. Its only really suitable for smallish amounts of
  // data - and checks to ensure that file size is less than MAX_CACHED_FILESIZE
     char *METHOD(FileLikeObject, get_data);

// This method is just like the standard ftruncate call
     int METHOD(FileLikeObject, truncate, uint64_t offset);

// This closes the FileLikeObject and also frees it - it is not valid
// to use the FileLikeObject after calling this (it gets free'd).
     DESTRUCTOR void METHOD(FileLikeObject, close);
END_CLASS

// This file like object is backed by a real disk file:
CLASS(FileBackedObject, FileLikeObject)
     int fd;
END_CLASS

#endif 	    /* !AFF4_IO_H_ */
