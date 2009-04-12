#include "zip.h"

static AFFObject MapDriver_finish(AFFObject self) {
  char *stored = CALL(oracle, resolve, URNOF(self), AFF4_STORED);
  MapDriver this = (MapDriver)self;

  if(!stored) {
    RaiseError(ERuntimeError, "Map object does not have a stored attribute?");
    goto error;
  };

  self = CALL((AFFObject)this, Con, URNOF(self),'w');

  // Check that we have a stored property
  return self;
 error:
  talloc_free(self);
  return NULL;
};

/*** This is the implementation of the MapDriver */
static AFFObject MapDriver_Con(AFFObject self, char *uri, char mode){ 
  MapDriver this = (MapDriver)self;

  // Try to parse existing map object
  if(uri) {
    Blob blob;
    char buff[BUFF_SIZE];
    int blocksize;

    URNOF(self) = talloc_strdup(self, uri);
    this->parent_urn = CALL(oracle, resolve, uri, AFF4_STORED);
    if(!this->parent_urn) {
      RaiseError(ERuntimeError, "No storage for map %s?", uri);
      goto error;
    };

    CALL(oracle, set, URNOF(self), AFF4_TYPE, AFF4_MAP);
    blocksize = parse_int(resolver_get_with_default(oracle, 
				     URNOF(self), AFF4_BLOCKSIZE, "1"));
    
    // Load some parameters
    this->image_period = blocksize * parse_int(CALL(oracle, resolve, URNOF(self),
						   AFF4_IMAGE_PERIOD));
    
    this->target_period =  blocksize * parse_int(CALL(oracle, resolve, URNOF(self),
						     AFF4_TARGET_PERIOD));
    if(!this->target_period) this->target_period=-1;
    if(!this->image_period) this->image_period=-1;
    
    ((FileLikeObject)self)->size = parse_int(CALL(oracle, resolve, URNOF(self),
						  AFF4_SIZE));

    /** Try to load the map from the stream */
    snprintf(buff, BUFF_SIZE, "%s/map", uri);
    blob = (Blob)CALL(oracle, open, NULL, buff, 'r');
    if(blob) {
      char *map = talloc_strdup(self, blob->data);
      char *x=map, *y;
      struct map_point point;
      char target[1024];

      while(strlen(x)>0) {
	// Look for the end of line and null terminate it:
	y= x + strcspn(x, "\r\n");
	*y=0;
	if(sscanf(x,"%lld,%lld,%1000s", &point.image_offset, 
		  &point.target_offset, target)==3) {
	  CALL(this, add, point.target_offset*blocksize, 
	       point.image_offset*blocksize, target);
	};
	x=y+1;
      };      
    };
  } else {
    this->__super__->super.Con(self, NULL, mode);
  };

  return self;
 error:
  talloc_free(self);
  return NULL;
};
  
static int compare_points(const void *X, const void *Y) {
  struct map_point *x=(struct map_point *)X;
  struct map_point *y=(struct map_point *)Y;

  return x->target_offset - y->target_offset;
};

static void MapDriver_add(MapDriver self, uint64_t image_offset, uint64_t target_offset,
		   char *target_urn) {
  struct map_point new_point;
  MapDriver this=self;

  new_point.target_offset = target_offset;
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
static void MapDriver_save_map(MapDriver self) {
  char buff[BUFF_SIZE];
  struct map_point *point;
  int i;
  FileLikeObject fd;
  ZipFile zipfile = (ZipFile)CALL(oracle, open, NULL, self->parent_urn, 'w');

  if(!zipfile) return;
  snprintf(buff, BUFF_SIZE, "%s/map", URNOF(self));

  fd = CALL(zipfile, open_member, buff, 'w', NULL, 0, ZIP_DEFLATE);
  for(i=0;i<self->number_of_points;i++) {
    point = &self->points[i];
    snprintf(buff, BUFF_SIZE, "%lld,%lld,%s\n", point->target_offset, 
	     point->image_offset, point->target_urn);
    CALL(fd, write, ZSTRING_NO_NULL(buff));
  };

  CALL(fd, close);
};

static void MapDriver_close(FileLikeObject self) {
  MapDriver this = (MapDriver)self;

  // Write out a properties file
  char *properties = CALL(oracle, export_urn, URNOF(self));
  if(properties) {
    ZipFile zipfile = (ZipFile)CALL(oracle, open, NULL, this->parent_urn, 'w');
    char tmp[BUFF_SIZE];

    snprintf(tmp, BUFF_SIZE, "%s/properties", URNOF(self));
    CALL((ZipFile)zipfile, writestr, tmp, ZSTRING_NO_NULL(properties),
	 NULL, 0, ZIP_STORED);

    talloc_free(properties);
    // Done with zipfile
    CALL(oracle, cache_return, (AFFObject)zipfile);
  };
};




// searches the array of map points and returns the offset in the
// array such that array[result].file_offset > offset
static int bisect_left(uint64_t offset, struct map_point *array, int hi) {
  uint64_t lo=0;
  uint64_t mid;

  while(lo < hi) {
    mid = (lo+hi)/2;
    if (array[mid].image_offset <= offset) {
      lo = mid+1;
    } else {
      hi = mid;
    };
  };
  return lo;
};

static int bisect_right(uint64_t offset, struct map_point *array, int hi) {
  uint64_t lo=0;
  uint64_t mid;

  while(lo < hi) {
    mid = (lo+hi)/2;
    if (offset < array[mid].image_offset) {
      hi = mid;
    } else {
      lo = mid+1;
    };
  };

  return lo-1;
};

// Read as much as possible and return how much was read
static int MapDriver_partial_read(FileLikeObject self, char *buffer, \
				  unsigned long int length) {
  MapDriver this = (MapDriver)self;

  // How many periods we are from the start
  uint64_t period_number = self->readptr / this->image_period;

  // How far into this period we are within the image
  uint64_t image_period_offset = self->readptr % this->image_period;
  char direction = 'f';
  
  // The offset within the target we ultimately need
  uint64_t target_offset;
  uint64_t available_to_read = self->size - self->readptr;
  FileLikeObject target;
  int read_bytes;
  int l;

  // We can't interpolate forward before the first point - must
  // interpolate backwards.
  if(image_period_offset < this->points[0].image_offset) {
    direction = 'r';
  };

  /** Interpolate forward */
  if(direction=='f') {
    l = bisect_right(image_period_offset, this->points, this->number_of_points);
    
    // Here this->points[l].image_offset < image_period_offset
    target_offset = this->points[l].target_offset +			\
      image_period_offset - this->points[l].image_offset + period_number * this->target_period;
    
    if(l < this->number_of_points-1) {
      available_to_read = this->points[l+1].image_offset - image_period_offset;
    } else {
      available_to_read = min(available_to_read, this->image_period - image_period_offset);
    };

    /*
    printf("%lld %lld %d ", self->readptr, image_period_offset, l);
    printf(" target %lld available %d\n", target_offset, available_to_read);
    */

    /** Interpolate in reverse */
  } else {
    l = bisect_left(image_period_offset, this->points, this->number_of_points);
    target_offset = this->points[l].target_offset -	\
      (this->points[l].image_offset - image_period_offset) + \
      period_number * this->target_period;
    
    if(l<this->number_of_points) {
      available_to_read = this->points[l].image_offset - image_period_offset;
    };
  };

  available_to_read = min(available_to_read, length);

  // Now do the read:
  target = (FileLikeObject)CALL(oracle, open, NULL, this->points[l].target_urn, 'r');
  if(!target) return -1;

  CALL(target, seek, target_offset, SEEK_SET);
  read_bytes = CALL(target, read, buffer, available_to_read);

  CALL(oracle, cache_return, (AFFObject)target);

  if(read_bytes >0) {
    ((FileLikeObject)self)->readptr += read_bytes;
  };

  return read_bytes;
};

static int MapDriver_read(FileLikeObject self, char *buffer, unsigned long int length) {
  int i=0;
  int read_length;

  // Clip the read to the stream size
  length = min(length, self->size - self->readptr);

  // Just execute as many partial reads as are needed to satisfy the
  // length requested
  while(i < length ) {
    read_length = MapDriver_partial_read(self, buffer + i, length - i);
    if(read_length <=0) break;
    i += read_length;
  };

  return i;
};


VIRTUAL(MapDriver, FileLikeObject)
     VMETHOD(super.super.Con) = MapDriver_Con;
     VMETHOD(super.super.finish) = MapDriver_finish;
     VMETHOD(add) = MapDriver_add;
     VMETHOD(save_map) = MapDriver_save_map;
     VMETHOD(super.read) = MapDriver_read;  
     VMETHOD(super.close) = MapDriver_close;
END_VIRTUAL
