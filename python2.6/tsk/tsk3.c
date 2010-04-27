/*
** tsk3.c
** 
** Made by (mic)
** Login   <mic@laptop>
** 
** Started on  Fri Apr 16 10:01:04 2010 mic
** Last update Sun May 12 01:17:25 2002 Speed Blue
*/

#include "tsk3.h"


/** This macro is used to receive the object reference from a
    member of the type.
*/
#define GET_Object_from_member(type, object, member)                    \
  (type)(((char *)object) - (unsigned long)(&((type)0)->member))

void IMG_INFO_close(TSK_IMG_INFO *img) {
  Extended_TSK_IMG_INFO self = (Extended_TSK_IMG_INFO)img;

  CALL(self->container, close);
};

ssize_t IMG_INFO_read(TSK_IMG_INFO *img, TSK_OFF_T off, char *buf, size_t len) {
  Extended_TSK_IMG_INFO self = (Extended_TSK_IMG_INFO)img;

  return (ssize_t)CALL(self->container, read, (uint64_t)off, buf, len);
};

AFF4ImgInfo AFF4ImgInfo_Con(AFF4ImgInfo self, char *urn, TSK_OFF_T offset) {
  FileLikeObject fd;

  self->urn = new_RDFURN(self);
  CALL(self->urn, set, urn);

  self->offset = offset;

  // Try to open it for reading just to make sure its ok:
  fd = (FileLikeObject)CALL(oracle, open, self->urn, 'r');
  if(!fd) goto error;

  // Initialise the img struct with the correct callbacks:
  self->img = talloc_zero(self, struct Extended_TSK_IMG_INFO_t);
  self->img->container = self;

  self->img->base.read = IMG_INFO_read;
  self->img->base.close = IMG_INFO_close;
  self->img->base.size = fd->size->value;
  self->img->base.sector_size = 512;
  self->img->base.itype = TSK_IMG_TYPE_RAW_SING;

  CALL((AFFObject)fd, cache_return);

  return self;

 error:
  talloc_free(self);
  return NULL;
};

ssize_t AFF4ImgInfo_read(AFF4ImgInfo self, TSK_OFF_T off, OUT char *buf, size_t len) {
  FileLikeObject fd = (FileLikeObject)CALL(oracle, open, self->urn, 'r');

  if(fd) {
    ssize_t res;

    CALL(fd, seek, off, SEEK_SET);
    res = CALL(fd, read, buf, len);
    CALL((AFFObject)fd, cache_return);

    return res;
  };

  return -1;
};

// Dont really do anything here
void AFF4ImgInfo_close(AFF4ImgInfo self) {

};

VIRTUAL(AFF4ImgInfo, Object) {
  VMETHOD(Con) = AFF4ImgInfo_Con;
  VMETHOD(read) = AFF4ImgInfo_read;
  VMETHOD(close) = AFF4ImgInfo_close;
} END_VIRTUAL


int FS_Info_dest(void *this) {
  FS_Info self = (FS_Info)this;

  tsk_fs_close(self->info);
  return 0;
};

static FS_Info FS_Info_Con(FS_Info self, AFF4ImgInfo img, TSK_FS_TYPE_ENUM type) {
  // Now try to open the filesystem
  self->info = tsk_fs_open_img((struct TSK_IMG_INFO *)img->img, img->offset, type);
  if(!self->info) {
    RaiseError(ERuntimeError, "Unable to open the image as a filesystem: %s",
               tsk_error_get());
    goto error;
  };

  // Make sure that the filesystem is properly closed when we get freed
  talloc_set_destructor((void *)self, FS_Info_dest);
  return self;

 error:
  talloc_free(self);
  return NULL;
};

static Directory FS_Info_open_dir(FS_Info self, ZString path, TSK_INUM_T inode) {
  return CONSTRUCT(Directory, Directory, Con, NULL, self, path, inode);
};

static File FS_Info_open(FS_Info self, ZString path) {
  TSK_FS_FILE *handle = tsk_fs_file_open(self->info, NULL, path);
  File result;

  if(!handle) {
    RaiseError(ERuntimeError, "Cant open file: %s", tsk_error_get());
    return NULL;
  };

  result = CONSTRUCT(File, File, Con, NULL, handle);

  return result;
};

static File FS_Info_open_meta(FS_Info self, TSK_INUM_T inode) {
  TSK_FS_FILE *handle = tsk_fs_file_open_meta(self->info, NULL, inode);
  File result;

  if(!handle) {
    RaiseError(ERuntimeError, "Cant open file: %s", tsk_error_get());
    return NULL;
  };

  result = CONSTRUCT(File, File, Con, NULL, handle);

  return result;
};

VIRTUAL(FS_Info, Object) {
  VMETHOD(Con) = FS_Info_Con;
  VMETHOD(open_dir) = FS_Info_open_dir;
  VMETHOD(open) = FS_Info_open;
  VMETHOD(open_meta) = FS_Info_open_meta;
} END_VIRTUAL


static int Directory_dest(void *self) {
  Directory this = (Directory)self;
  tsk_fs_dir_close(this->info);

  return 0;
};

static Directory Directory_Con(Directory self, FS_Info fs,
                               ZString path, TSK_INUM_T inode) {
  if(!path) {
    self->info = tsk_fs_dir_open_meta(fs->info, inode);
  } else {
    self->info = tsk_fs_dir_open(fs->info, path);
  };

  if(!self->info) {
    RaiseError(ERuntimeError, "Unable to open directory: %s", tsk_error_get());
    goto error;
  };

  self->current = 0;
  self->size = tsk_fs_dir_getsize(self->info);
  talloc_set_destructor((void *)self, Directory_dest);

  return self;

 error:
  talloc_free(self);
  return NULL;
};

static File Directory_next(Directory self) {
  File result;
  TSK_FS_FILE *info;

  if(self->current >= self->size) {
    return NULL;
  };

  info = tsk_fs_dir_get(self->info, self->current);
  if(!info) {
    RaiseError(ERuntimeError, "Error opening File: %s", tsk_error_get());
    return NULL;
  };

  result = CONSTRUCT(File, File, Con, NULL, info);
  result->info = info;
  self->current ++;

  return result;
};

static void Directory_iter(Directory self) {
  self->current = 0;
};

VIRTUAL(Directory, Object) {
  VMETHOD(Con) = Directory_Con;
  VMETHOD(iternext) = Directory_next;
  VMETHOD(__iter__) = Directory_iter;
} END_VIRTUAL

static int File_dest(void *self) {
  File this = (File)self;
  if(this->info) {
    tsk_fs_file_close(this->info);
  };

  return 0;
};


static File File_Con(File self, TSK_FS_FILE *info) {
  self->info = info;

  // Get the total number of attributes:
  self->max_attr = tsk_fs_file_attr_getsize(info);

  talloc_set_destructor((void *)self, File_dest);
  return self;
};

static ssize_t File_read_random(File self, TSK_OFF_T offset,
                                OUT char *buff, int len,
                                TSK_FS_ATTR_TYPE_ENUM type, int id,
                                TSK_FS_FILE_READ_FLAG_ENUM flags) {
  ssize_t result;

  if(id >=0) {
    result = tsk_fs_file_read_type(self->info, type, id, offset, buff, len, flags);
  } else {
    result = tsk_fs_file_read(self->info, offset, buff, len, flags);
  };

  if(result < 0) {
    RaiseError(ERuntimeError, "Read error: %s", tsk_error_get());
    return 0;
  };

  return result;
};

static Attribute File_iternext(File self) {
  TSK_FS_ATTR *attribute;
  Attribute result;

  if(self->current_attr >= self->max_attr) {
    return NULL;
  };

  attribute = (TSK_FS_ATTR *)tsk_fs_file_attr_get_idx(self->info, self->current_attr);
  if(!attribute)  {
    RaiseError(ERuntimeError, "Error opening File: %s", tsk_error_get());
    return NULL;
  };

  result = CONSTRUCT(Attribute, Attribute, Con, NULL, attribute);
  self->current_attr ++;

  return result;
};

static void File_iter__(File self) {
  self->current_attr = 0;
};

VIRTUAL(File, Object) {
  VMETHOD(Con) = File_Con;
  VMETHOD(read_random) = File_read_random;
  VMETHOD(iternext) = File_iternext;
  VMETHOD(__iter__) = File_iter__;
} END_VIRTUAL

static Attribute Attribute_Con(Attribute self, TSK_FS_ATTR *info) {
  self->info = info;

  return self;
};

static void Attribute_iter(Attribute self) {
  self->current = self->info->nrd.run;
};

static TSK_FS_ATTR_RUN *Attribute_iternext(Attribute self) {
  TSK_FS_ATTR_RUN *result = self->current;

  if(result) {
    self->current = self->current->next;
    if(self->current == self->info->nrd.run){
      self->current = NULL;
    };

    result = talloc_memdup(NULL, result, sizeof(*result));
  };

  return result;
};

VIRTUAL(Attribute, Object) {
  VMETHOD(Con) = Attribute_Con;
  VMETHOD(iternext) = Attribute_iternext;
  VMETHOD(__iter__) = Attribute_iter;
} END_VIRTUAL