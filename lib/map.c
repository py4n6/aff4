#include "aff4.h"

/*** This is the implementation of the MapDriver */
static AFFObject MapDriver_Con(AFFObject self, RDFURN uri, char mode){ 
  MapDriver this = (MapDriver)self;

  // Try to parse existing map object
  if(uri) {
    FileLikeObject fd;
    XSDInteger blocksize = (XSDInteger)rdfvalue_from_int(NULL, 1);

    URNOF(self) = CALL(uri, copy, self);

    this->stored = new_RDFURN(self);
    this->target_urn = new_RDFURN(self);
    this->target_period = new_XSDInteger(self);
    this->image_period = new_XSDInteger(self);
    this->blocksize = new_XSDInteger(self);
    this->map_urn = CALL(uri, copy, self);
    CALL(this->map_urn, add, "map");

    // Set defaults
    this->blocksize->value = 1;
    this->image_period->value = -1;
    this->target_period->value = -1;

    // Make a target cache
    this->targets = CONSTRUCT(Cache, Cache, Con, self, 100, 0);

    // Check that we have a stored property
    if(!CALL(oracle, resolve_value, URNOF(self), AFF4_STORED, 
	     (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "Map object does not have a stored attribute?");
      goto error;
    };

    CALL(oracle, set_value, URNOF(self), AFF4_TYPE, rdfvalue_from_string(self, AFF4_MAP));
    CALL(oracle, add_value, this->stored, AFF4_VOLATILE_CONTAINS, (RDFValue)uri);

    // Only update the timestamp if we created a new map
    if(mode=='w') {
      XSDDatetime time = new_XSDDateTime(this);

      CALL(oracle, set_value, URNOF(self), AFF4_TIMESTAMP, (RDFValue)time);
    };
    CALL(oracle, resolve_value, URNOF(self), AFF4_BLOCKSIZE,
	 (RDFValue)blocksize);

    // Load some parameters
    CALL(oracle, resolve_value, URNOF(self), AFF4_IMAGE_PERIOD,
	 (RDFValue)this->image_period);

    CALL(oracle, resolve_value, URNOF(self), AFF4_TARGET_PERIOD,
	 (RDFValue)this->target_period);

    this->image_period->value *= blocksize->value;
    this->target_period->value *= blocksize->value;

    ((FileLikeObject)self)->size = new_XSDInteger(self);
    CALL(oracle, resolve_value, URNOF(self), AFF4_SIZE,
	 (RDFValue)((FileLikeObject)self)->size);

    /** Try to load the map from the stream */
    fd = (FileLikeObject)CALL(oracle, open, (RDFURN)this->map_urn, 'r');
    if(fd) {
      char *map = talloc_strdup(self, CALL(fd,get_data));
      char *x=map, *y;
      struct map_point point;
      char target[1024];

      while(strlen(x)>0) {
	// Look for the end of line and null terminate it:
	y= x + strcspn(x, "\r\n");
	*y=0;
	if(sscanf(x,"%lld,%lld,%1000s", &point.image_offset,
		  &point.target_offset, target)==3) {
	  CALL(this, add, point.image_offset * blocksize->value,
	       point.target_offset * blocksize->value, target);
	};
	x=y+1;
      };

      CALL(oracle, cache_return, (AFFObject)fd);
    } else if(mode == 'r') {
      goto error;
    };

    // Ignore it if we cant open the map_urn
    ClearError();
  };

  self = SUPER(AFFObject, FileLikeObject, Con, uri, mode);

  return self;
 error:
  talloc_free(self);
  return NULL;
};
  
static int compare_points(const void *X, const void *Y) {
  struct map_point *x=(struct map_point *)X;
  struct map_point *y=(struct map_point *)Y;

  return x->image_offset - y->image_offset;
};

static void MapDriver_add(MapDriver self, uint64_t image_offset, uint64_t target_offset,
			  char *target) {
  struct map_point new_point;
  MapDriver this=self;
  // Do we already have this target in the cache?
  RDFURN target_urn = (RDFURN)CALL(self->targets, borrow, ZSTRING_NO_NULL(target));

  if(!target_urn) {
    target_urn = new_RDFURN(self->targets);
    CALL(target_urn, set, target);

    // Store it in the cache
    CALL(self->targets, put, ZSTRING_NO_NULL(target), (Object)target_urn);
  };

  new_point.target_offset = target_offset;
  new_point.image_offset = image_offset;
  new_point.target_urn = target_urn;

  // Now append the new point to our struct:
  this->points = talloc_realloc(self, this->points,
				struct map_point,
				(this->number_of_points + 1) * sizeof(*this->points));
  memcpy(&this->points[this->number_of_points], &new_point, sizeof(new_point));

  this->number_of_points ++;

  if(self->super.size->value < new_point.image_offset) {
    self->super.size->value = new_point.image_offset;
  };
};

static void MapDriver_write_from(MapDriver self, RDFURN target, uint64_t target_offset, uint64_t target_length) {
  FileLikeObject this = (FileLikeObject)self;

  CALL(self, add, this->readptr, target_offset, target->value);
  this->readptr += target_length;
  this->size->value = max(this->size->value, this->readptr);
};

// This writes out the map to the stream
static void MapDriver_save_map(MapDriver self) {
  MapDriver this = self;
  struct map_point *point;
  int i;
  FileLikeObject fd;
  AFF4Volume zipfile = (AFF4Volume)CALL(oracle, open, self->stored, 'w');

  if(!zipfile) return;

  // Now sort the array
  qsort(this->points, this->number_of_points, sizeof(*this->points),
	compare_points);

  fd = CALL(zipfile, open_member, self->map_urn->value, 'w', ZIP_DEFLATE);
  if(!fd)
    goto error;

  for(i=0;i<self->number_of_points;i++) {
    char buff[BUFF_SIZE];

    point = &self->points[i];
    // See if we can reduce adjacent points
    // Note that due to the cache we can guarantee that if the
    // target_urn pointers are different, they are in fact different
    // URNs.
    if(i!=0 && i!=self->number_of_points && 
       point->target_urn == self->points[i-1].target_urn) {
      struct map_point *previous = &self->points[i-1];
      uint64_t prediction = point->image_offset - previous->image_offset + 
	previous->target_offset;

      // Dont write this point if its linearly related to previous point.
      if(prediction == point->target_offset) continue;
    };

    snprintf(buff, BUFF_SIZE, "%lld,%lld,%s\n", point->image_offset,
	     point->target_offset, point->target_urn->value);
    CALL(fd, write, ZSTRING_NO_NULL(buff));
  };

  CALL(fd, close);

  CALL(oracle, set_value, URNOF(self), AFF4_SIZE, (RDFValue)self->super.size);
 error:
  CALL(oracle, cache_return, (AFFObject)zipfile);
};

static void MapDriver_close(FileLikeObject self) {
  MapDriver this = (MapDriver)self;

  MapDriver_save_map(this);

  talloc_free(self);
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

static void MapDriver_get_range(MapDriver this, uint64_t *image_offset_at_point,
                     uint64_t *target_offset_at_point,
                     uint64_t *available_to_read,
                     RDFURN urn) {
  FileLikeObject self = (FileLikeObject)this;
  // How many periods we are from the start
  uint64_t period_number = self->readptr / (uint64_t)this->image_period->value;
  // How far into this period we are within the image
  uint64_t image_period_offset = self->readptr % (uint64_t)this->image_period->value;
  char direction = 'f';
  int l;

  *available_to_read = self->size->value - self->readptr;

  if(*available_to_read == 0) {
    *target_offset_at_point = 0;
    *image_offset_at_point = 0;

    return;
  };

  // We can't interpolate forward before the first point - must
  // interpolate backwards.
  if(image_period_offset < this->points[0].image_offset) {
    direction = 'r';
  };

  /** Interpolate forward */
  if(direction=='f') {
    l = bisect_right(image_period_offset, this->points, this->number_of_points);

    // Here this->points[l].image_offset < image_period_offset
    *target_offset_at_point = this->points[l].target_offset +   \
      image_period_offset - this->points[l].image_offset +      \
      period_number * this->target_period->value;

    if(l < this->number_of_points-1) {
      *available_to_read = this->points[l+1].image_offset -     \
        image_period_offset;
    } else {
      *available_to_read = min(*available_to_read, this->image_period->value -
                               image_period_offset);
    };

    /** Interpolate in reverse */
  } else {
    l = bisect_left(image_period_offset, this->points, this->number_of_points);
    *target_offset_at_point = this->points[l].target_offset -   \
      (this->points[l].image_offset - image_period_offset) +    \
      period_number * this->target_period->value;

    if(l<this->number_of_points) {
      *available_to_read = this->points[l].image_offset - image_period_offset;
    } else {
      RaiseError(ERuntimeError, "Something went wrong...");
      return;
    };
  };

  *image_offset_at_point = self->readptr;

  if(urn)
    CALL(urn, set, this->points[l].target_urn->value);

  return;
};

// Read as much as possible and return how much was read
static int MapDriver_partial_read(FileLikeObject self, char *buffer, \
				  unsigned long int length) {
  MapDriver this = (MapDriver)self;
  FileLikeObject target;
  int read_bytes;
  uint64_t image_offset_at_point, target_offset_at_point, available_to_read;

  MapDriver_get_range(this, &image_offset_at_point, &target_offset_at_point, &available_to_read,
                      this->target_urn);

  // Clamp the available_to_read to the length requested
  available_to_read = min(available_to_read, length);

  // Now do the read:
  target = (FileLikeObject)CALL(oracle, open,  this->target_urn, 'r');
  if(!target) return -1;

  CALL(target, seek, target_offset_at_point, SEEK_SET);
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
  length = min(length, self->size->value - self->readptr);

  // Just execute as many partial reads as are needed to satisfy the
  // length requested
  while(i < length ) {
    read_length = MapDriver_partial_read(self, buffer + i, length - i);
    if(read_length==0) break;
    if(read_length < 0) {
      // An error occurred - we need to decide if to pad or not
      /*
      char *pad = (char *)CALL(oracle, resolve, self, 
			       CONFIGURATION_NS, CONFIG_PAD,
			       RESOLVER_DATA_STRING);
      */
      int pad = 1;

      if(pad) {
	memset(buffer + i ,0, length -i);
      } else {
	PrintError();
	return -1;
      };
    };
    i += read_length;
  };

  return i;
};


VIRTUAL(MapDriver, FileLikeObject) {
     VMETHOD_BASE(AFFObject, Con) = MapDriver_Con;
     VMETHOD_BASE(AFFObject, dataType) = AFF4_MAP;

     VMETHOD(add) = MapDriver_add;
     VMETHOD(write_from) = MapDriver_write_from;
     VMETHOD(save_map) = MapDriver_save_map;
     VMETHOD(get_range) = MapDriver_get_range;
     VMETHOD_BASE(FileLikeObject, read) = MapDriver_read;
     VMETHOD_BASE(FileLikeObject, close) = MapDriver_close;
} END_VIRTUAL

void mapdriver_init() {
  INIT_CLASS(MapDriver);

  register_type_dispatcher(AFF4_MAP, (AFFObject *)GETCLASS(MapDriver));
}
