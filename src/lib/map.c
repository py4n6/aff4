#include "zip.h"

AFFObject MapDriver_finish(AFFObject self) {
  char *stored = CALL(oracle, resolve, URNOF(self), "aff2:stored");
  MapDriver this = (MapDriver)self;

  if(!stored) {
    RaiseError(ERuntimeError, "Map object does not have a stored attribute?");
    goto error;
  };

  self = CALL((AFFObject)this, Con, URNOF(self));

  // Check that we have a stored property
  return self;
 error:
  talloc_free(self);
  return NULL;
};

/*** This is the implementation of the MapDriver */
AFFObject MapDriver_Con(AFFObject self, char *uri){ 
  MapDriver this = (MapDriver)self;

  // Try to parse existing map object
  if(uri) {
    Blob blob;
    char buff[BUFF_SIZE];

    URNOF(self) = talloc_strdup(self, uri);
    this->parent_urn = CALL(oracle, resolve, uri, "aff2:stored");
    if(!this->parent_urn) {
      RaiseError(ERuntimeError, "No storage for map %s?", uri);
      goto error;
    };

    /** Try to load the map from the stream */
    snprintf(buff, BUFF_SIZE, "%s/map", uri);
    blob = (Blob)CALL(oracle, open, NULL, buff);
    if(blob) {
      char *map = talloc_strdup(self, blob->data);
      char *x=map, *y;
      struct map_point point;
      char target[1024];
    
      while(strlen(x)>0) {
	// Look for the end of line and null terminate it:
	y= x + strcspn(x, "\r\n");
	*y=0;
	if(sscanf(x,"%lld,%lld,%1000s", &point.file_offset, 
		  &point.image_offset, target)==3) {
	  CALL(this, add, point.file_offset, point.image_offset, target);
	};
	x=y+1;
      };      
    };
  } else {
    this->__super__->super.Con(self, NULL);
  };

  return self;
 error:
  talloc_free(self);
  return NULL;
};
  
static int compare_points(const void *X, const void *Y) {
  struct map_point *x=(struct map_point *)X;
  struct map_point *y=(struct map_point *)Y;

  return x->file_offset - y->file_offset;
};

void MapDriver_add(MapDriver self, uint64_t file_pos, uint64_t image_offset, 
		   char *target_urn) {
  int i,found=0;
  struct map_point new_point;
  MapDriver this=self;

  new_point.file_offset = file_pos;
  new_point.image_offset = image_offset;
  new_point.target_urn = talloc_strdup(this->points, target_urn);

  // Now append the new point to our struct:
  this->points = talloc_realloc(self, this->points, 
				struct map_point, 
				(this->number_of_points + 1) * sizeof(*this->points));
  memcpy(&this->points[this->number_of_points], &new_point, sizeof(new_point));

  // Now sort the array
  qsort(this->points, this->number_of_points, sizeof(*this->points),
	compare_points);
  this->number_of_points ++;
};

// This writes out the map to the stream
void MapDriver_save_map(MapDriver self) {
  char buff[BUFF_SIZE];
  struct map_point *point;
  int i;
  FileLikeObject fd;
  ZipFile zipfile = CALL(oracle, open, NULL, self->parent_urn);

  if(!zipfile) return;
  snprintf(buff, BUFF_SIZE, "%s/map", URNOF(self));

  fd = CALL(zipfile, open_member, buff, 'w', NULL, 0, ZIP_DEFLATE);
  for(i=0;i<self->number_of_points;i++) {
    point = &self->points[i];
    snprintf(buff, BUFF_SIZE, "%lld,%lld,%s\n", point->file_offset, point->image_offset, point->target_urn);
    CALL(fd, write, ZSTRING_NO_NULL(buff));
  };

  CALL(fd, close);
};

void MapDriver_close(FileLikeObject self) {
  MapDriver this = (MapDriver)self;

  // Write out a properties file
  char *properties = CALL(oracle, export, URNOF(self));
  if(properties) {
    ZipFile zipfile = (ZipFile)CALL(oracle, open, NULL, this->parent_urn);
    char tmp[BUFF_SIZE];

    snprintf(tmp, BUFF_SIZE, "%s/properties", URNOF(self));
    CALL((ZipFile)zipfile, writestr, tmp, ZSTRING_NO_NULL(properties),
	 NULL, 0, ZIP_STORED);

    talloc_free(properties);
    // Done with zipfile
    CALL(oracle, cache_return, (AFFObject)zipfile);
  };
};

VIRTUAL(MapDriver, FileLikeObject)
     VMETHOD(super.super.Con) = MapDriver_Con;
     VMETHOD(super.super.finish) = MapDriver_finish;
     VMETHOD(add) = MapDriver_add;
     VMETHOD(save_map) = MapDriver_save_map;
     VMETHOD(super.close) = MapDriver_close;
END_VIRTUAL
