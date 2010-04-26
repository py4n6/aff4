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
BIND_STRUCT(TSK_FS_NAME);
BIND_STRUCT(TSK_FS_FILE);
BIND_STRUCT(TSK_FS_BLOCK);
BIND_STRUCT(TSK_FS_ATTR);
BIND_STRUCT(TSK_FS_ATTR_RUN);

BIND_STRUCT(TSK_FS_ATTR_TYPE_ENUM);

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

// Forward declerations
struct FS_Info_t;

CLASS(Attribute, Object)
   FOREIGN TSK_FS_ATTR *info;
   FOREIGN TSK_FS_ATTR_RUN *current;

   Attribute METHOD(Attribute, Con, TSK_FS_ATTR *info);

   void METHOD(Attribute, __iter__);
   TSK_FS_ATTR_RUN *METHOD(Attribute, iternext);
END_CLASS


   /** This represents a file object. A file has both metadata and
       data streams.

       Its usually not useful to instantiate this class by itself -
       you need to call FS_Info.open() or iterate over a Directory()
       object.

       This object may be used to read the content of the file using
       read_random().

       Iterating over this object will return all the attributes for
       this file.
   */
CLASS(File, Object)
     FOREIGN TSK_FS_FILE *info;

     int max_attr;
     int current_attr;

     File METHOD(File, Con, TSK_FS_FILE *info);

     /** Read a buffer from a random location in the file.

         DEFAULT(flags) = 0;
         DEFAULT(type) = TSK_FS_ATTR_TYPE_DEFAULT;
         DEFAULT(id) = -1;
     */
     ssize_t METHOD(File, read_random, TSK_OFF_T offset,
                    OUT char *buff, int len,
                    TSK_FS_ATTR_TYPE_ENUM type, int id,
                    TSK_FS_FILE_READ_FLAG_ENUM flags);

     void METHOD(File, __iter__);
     Attribute METHOD(File, iternext);
END_CLASS

     /** This represents a Directory within the filesystem. You can
         iterate over this object to obtain all the File objects
         contained within this directory:

         for f in d:
            print f.info.name.name
     */
CLASS(Directory, Object)
     FOREIGN TSK_FS_DIR *info;

     // Total number of files in this directory
     size_t size;

     // Current file returned in the next iteration
     int current;

     /* We can open the directory using a path, its inode number.

        DEFAULT(path) = NULL;
        DEFAULT(inode) = 0;
      */
     Directory METHOD(Directory, Con, struct FS_Info_t *fs, ZString path, TSK_INUM_T inode);

     /** An iterator of all files in the present directory. */
     void METHOD(Directory, __iter__);
     File METHOD(Directory, iternext);
END_CLASS

/** This is used to obtain a filesystem object from an AFF4ImgInfo.

    From this FS_Info we can open files or directories by inode, or
    path.
 */
CLASS(FS_Info, Object)
     FOREIGN TSK_FS_INFO *info;

     /** Open the filesystem stored on image.

       DEFAULT(type) = TSK_FS_TYPE_DETECT;
     */
     FS_Info METHOD(FS_Info, Con, AFF4ImgInfo img, TSK_FS_TYPE_ENUM type);

     /** A convenience function to open a directory in this image. 

         DEFAULT(path) = NULL;
         DEFAULT(inode) = 2;
     */
     Directory METHOD(FS_Info, open_dir, ZString path, TSK_INUM_T inode);

     /** A convenience function to open a file in this image. */
     File METHOD(FS_Info, open, ZString path);

     // Open a file by inode number
     File METHOD(FS_Info, open_meta, TSK_INUM_T inode);

END_CLASS

#endif 	    /* !TSK3_H_ */
