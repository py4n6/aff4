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

typedef struct Extended_TSK_IMG_INFO_t {
  TSK_IMG_INFO base;
  struct AFF4ImgInfo_t *container;
} *Extended_TSK_IMG_INFO;

BIND_STRUCT(TSK_FS_INFO);

/** This is an image info object based on an AFF4 object.

    Before we can use libtsk we need to instantiate one of these from
    a base URN.
 */
CLASS(AFF4ImgInfo, Object)
   RDFURN urn;
   TSK_OFF_T offset;

   // This is used to create a new TSK_IMG_INFO for TSK to use:
   Extended_TSK_IMG_INFO img;

   /** Read the filesystem from the urn specified.

       DEFAULT(offset) = 0;
   */
   AFF4ImgInfo METHOD(AFF4ImgInfo, Con, ZString urn, TSK_OFF_T offset);

   // Read a random buffer from the image
   ssize_t METHOD(AFF4ImgInfo, read, TSK_OFF_T off, OUT char *buf, size_t len);

   // Closes the image
   void METHOD(AFF4ImgInfo, close);
END_CLASS


/** This is used to obtain a filesystem object from an AFF4ImgInfo */
CLASS(FS_Info, Object)
     FOREIGN TSK_FS_INFO *fs;

     /** Open the filesystem stored on image.

       DEFAULT(type) = TSK_FS_TYPE_DETECT;
     */
     FS_Info METHOD(FS_Info, Con, AFF4ImgInfo img, TSK_FS_TYPE_ENUM type);
END_CLASS

#endif 	    /* !TSK3_H_ */
