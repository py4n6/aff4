/*
** tsk3.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Fri Apr 16 10:01:14 2010 mic
** Last update Fri Apr 16 10:01:14 2010 mic
*/

#ifndef   	TSK3_H_
# define   	TSK3_H_
#include "class.h"
#include "aff4.h"
#include <tsk3/libtsk.h>

/** This is an image info object based on an AFF4 object.

    Before we can use libtsk we need to instantiate one of these from
    a base URN.
 */
CLASS(AFF4ImgInfo, Object)
   RDFURN urn;

   // This is used to create a new TSK_IMG_INFO for TSK to use:
   struct TSK_IMG_INFO img;

   struct TSK_FS_INFO *fs;

   /** Read the filesystem from the urn specified.

       DEFAULT(offset) = 0;
       DEFAULT(type) = 0;
   */
   AFF4ImgInfo METHOD(AFF4ImgInfo, Con, ZString urn, TSK_OFF_T offset, int type);

   // Read a random buffer from the image
   ssize_t METHOD(AFF4ImgInfo, read, TSK_OFF_T off, OUT char *buf, int len);

   // Closes the image
   void METHOD(AFF4ImgInfo, close);
END_CLASS

   /** This macro is used to receive the object reference from a
       member of the type.
   */
#define GET_Object_from_member(type, object, member)                    \
  (type)(((char *)object) - (unsigned long)(&((type)0)->member))

#endif 	    /* !TSK3_H_ */
