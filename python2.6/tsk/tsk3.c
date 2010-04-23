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

void IMG_INFO_close(TSK_IMG_INFO *img) {
  AFF4ImgInfo self = GET_Object_from_member(AFF4ImgInfo, img, img);

  CALL(self, close);
};

ssize_t IMG_INFO_read(TSK_IMG_INFO *img, TSK_OFF_T off, char *buf, size_t len) {
  AFF4ImgInfo self = GET_Object_from_member(AFF4ImgInfo, img, img);

  return (ssize_t)CALL(self, read, (uint64_t)off, buf, len);
};

AFF4ImgInfo AFF4ImgInfo_Con(AFF4ImgInfo self, char *urn, TSK_OFF_T offset, int type) {
  FileLikeObject fd;

  self->urn = new_RDFURN(self);
  CALL(self->urn, set, urn);

  // Try to open it for reading just to make sure its ok:
  fd = (FileLikeObject)CALL(oracle, open, self->urn, 'r');
  if(!fd) goto error;

  // Initialise the img struct with the correct callbacks:
  self->img.read = IMG_INFO_read;
  self->img.close = IMG_INFO_close;
  self->img.size = fd->size->value;
  self->img.sector_size = 512;
  self->img.itype = TSK_IMG_TYPE_RAW_SING;

  CALL((AFFObject)fd, cache_return);

  // Now try to open the filesystem
  self->fs = tsk_fs_open_img(&self->img, offset, type);
  if(!self->fs) {
    RaiseError(ERuntimeError, "Unable to open the image as a filesystem");
    goto error;
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};

ssize_t AFF4ImgInfo_read(AFF4ImgInfo self, TSK_OFF_T off, char *buf, size_t len) {
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
