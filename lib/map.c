#include "aff4.h"

  /* The comparison function for the treap. */
static int map_point_cmp(map_point_node_t *a, map_point_node_t *b) {
  int rVal = (a->image_offset > b->image_offset) -      \
    (a->image_offset < b->image_offset);

  return rVal;
};

/* This huge macro generates all the tree tranversal and searching
   functions.
*/
trp_gen(static, tree_, map_point_tree_t, map_point_node_t, head, \
        link, map_point_cmp, 1297, 1301);

static RDFValue MapValue_Con(RDFValue self) {
  MapValue this = (MapValue)self;

  this->urn = new_RDFURN(self);
  this->number_of_points = 0;
  this->number_of_urns = 0;
  this->cache = CONSTRUCT(Cache, Cache, Con, self, 100, 0);

  /* Initialize tree. */
  tree_new(&this->tree, 42);
  this->tree.value = this;

  this->targets = talloc_array(self, RDFURN, 1);

  this->size = new_XSDInteger(self);
  this->target_period = new_XSDInteger(self);
  this->image_period = new_XSDInteger(self);

  // Set defaults
  this->image_period->value = -1;
  this->target_period->value = -1;

  return self;
};

static int MapValue_decode(RDFValue self, char *data, int length, RDFURN subject) {
  MapValue this = (MapValue)self;
  FileLikeObject fd;
  RDFURN segment = new_RDFURN(self);

  CALL(this->urn, set, subject->value);

  // Load some parameters
  CALL(oracle, resolve_value, this->urn, AFF4_IMAGE_PERIOD,
       (RDFValue)this->image_period);

  CALL(oracle, resolve_value, this->urn, AFF4_TARGET_PERIOD,
       (RDFValue)this->target_period);

  CALL(oracle, resolve_value, this->urn, AFF4_SIZE,
       (RDFValue)this->size);

  CALL(segment,set, this->urn->value);
  CALL(segment, add, "map");

  // Now load our contents from the stored URN
  fd = (FileLikeObject)CALL(oracle, open, segment, 'r');
  if(!fd) goto error;
  else {
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
        CALL(this, add_point, point.image_offset,
             point.target_offset, target);
      };
      x=y+1;
    };
  };

  CALL(oracle, cache_return, (AFFObject)fd);

  talloc_free(segment);
  return 1;
 error:
  talloc_free(segment);
  return 0;
};

static map_point_node_t *text_map_iterate_cb(map_point_tree_t *tree,
                                       map_point_node_t *node,
                                       void *data) {
  FileLikeObject fd = (FileLikeObject)data;
  MapValue self = tree->value;
  char buff[BUFF_SIZE];
  int length;

  length = snprintf(buff, BUFF_SIZE, "%lld,%lld,%s\n", node->image_offset,
                    node->target_offset, self->targets[node->target_idx]->value);

  length = min(length, BUFF_SIZE);
  CALL(fd, write, buff, length);

  return NULL;
};

static char *MapValue_serialise(RDFValue self, RDFURN subject) {
  MapValue this = (MapValue)self;
  RDFURN stored = new_RDFURN(self);
  RDFURN segment = new_RDFURN(stored);
  AFF4Volume volume;
  FileLikeObject fd;

  CALL(segment, set, this->urn->value);
  CALL(segment, add, "map");

  // Check if the map segment already exists - if it does we dont
  // bother re-saving it.
  fd = (FileLikeObject)CALL(oracle, open, segment, 'r');
  if(fd) {
    CALL((AFFObject)fd, cache_return);
    goto exit;
  };

  // Work out where our map object is stored:
  if(!CALL(oracle, resolve_value, this->urn, AFF4_STORED, (RDFValue)stored)) {
    RaiseError(ERuntimeError, "No storage for map object");
    goto error;
  };

  volume = (AFF4Volume)CALL(oracle, open, stored, 'w');
  if(!volume) goto error;

  // Is it actually a volume?
  if(!ISSUBCLASS(volume, AFF4Volume)) {
    RaiseError(ERuntimeError, "URN %s is not a volume!!!", STRING_URNOF(volume));
    CALL((AFFObject)volume, cache_return);
    goto error;
  };

  fd = CALL(volume, open_member, segment->value, 'w', ZIP_DEFLATE);
  if(!fd) {
    goto error;
  };

  // Now save the map to the segment - Iterating over the treap in
  // forward order will produce a sorted array.
  tree_iter(&this->tree, NULL, text_map_iterate_cb, (void *)fd);

  // Done writing
  CALL((AFFObject)fd, close);

  // Done with our volume now
  CALL((AFFObject)volume, cache_return);

  // Now store various map parameters
  if(this->image_period->value != -1) {
    CALL(oracle, set_value, this->urn, AFF4_IMAGE_PERIOD, (RDFValue)this->image_period,0);
    CALL(oracle, set_value, this->urn, AFF4_TARGET_PERIOD, (RDFValue)this->target_period,0);
  };

  CALL(oracle, set_value, this->urn, AFF4_SIZE, (RDFValue)this->size,0);

 exit:
  return talloc_strdup(self, "");

 error:
  if(fd)
    talloc_free(fd);
  talloc_free(stored);
  return NULL;
};

static uint64_t MapValue_add_point(MapValue self, uint64_t image_offset, uint64_t target_offset,
                                   char *target) {
  // Do we already have this target in the cache?
  XSDInteger target_index = (XSDInteger)CALL(self->cache, borrow, ZSTRING(target));
  map_point_node_t *node = talloc_zero(self, map_point_node_t);
  uint64_t available_to_read=0;

  // Add another target to the array
  if(!target_index) {
    RDFURN target_urn = new_RDFURN(self);

    CALL(target_urn,set, target);

    target_index = new_XSDInteger(NULL);
    CALL(target_index, set, self->number_of_urns);

    self->targets = talloc_realloc_size(self, self->targets,
                                        (self->number_of_urns +1) *     \
                                        sizeof(*self->targets));
    self->targets[self->number_of_urns] = target_urn;

    self->number_of_urns ++ ;

    // Store it in the cache
    CALL(self->cache, put, ZSTRING(target), (Object)target_index);
    talloc_unlink(NULL, target_index);
  };

  node->image_offset = image_offset;
  node->target_offset = target_offset;
  node->target_idx = target_index->value;

  /** This might leak as we dont free anything here. Typically it
      does not matter because all nodes are allocated from the same
      slab.
  */
  // Now make sure that the current point is not already in the treap:
  {
    map_point_node_t q;
    map_point_node_t *old_node;

    memset(&q, 0, sizeof(q));
    q.image_offset = node->image_offset;
    old_node = tree_search(&self->tree, &q);
    if(old_node && old_node->image_offset == node->image_offset) {
      tree_remove(&self->tree, old_node);
      talloc_free(old_node);
      DEBUG_OBJECT("0x%X Removed point %llu %s while inserting %s\n", pthread_self(),
                   node->image_offset, self->targets[old_node->target_idx]->value,
                   self->targets[node->target_idx]->value);
    };
  };

  // Now check if the new point is redundant. The new point will be
  // redundant if the predicted target and target offset based on the
  // existing map points are the same as what we are about to add.
  {
    uint64_t old_target_offset;
    uint32_t existing_target_idx;
    RDFURN existing_target = CALL(self, get_range, image_offset, &old_target_offset,
                                  &available_to_read, &existing_target_idx);

    if(existing_target && existing_target_idx == target_index->value &&
       old_target_offset == target_offset) {
      goto error;
    };
    ClearError();
  };

  // Now add the new point:
  DEBUG_OBJECT("0x%X Adding point %llu %s\n", pthread_self(),
               node->image_offset, self->targets[node->target_idx]->value);

  tree_insert(&self->tree, node);
  self->number_of_points++;

  return available_to_read;

 error:
  talloc_free(node);
  return available_to_read;
};

static RDFURN MapValue_get_range(MapValue self, uint64_t readptr,
                                 uint64_t *target_offset_at_point,
                                 uint64_t *available_to_read,
                                 uint32_t *target_idx
                                 ) {
  // How many periods we are from the start
  uint64_t period_number = readptr / (self->image_period->value);
  // How far into this period we are within the image
  uint64_t image_period_offset = readptr % (self->image_period->value);
  map_point_node_t *pnode, *nnode, node;
  uint64_t next_offset;

  memset(&node, 0, sizeof(node));

  node.image_offset = readptr;

  pnode = tree_psearch(&self->tree, &node);

  node.image_offset += 1;
  nnode = tree_nsearch(&self->tree, &node);

  // Now we have a range for our point:
  if(!nnode) {
    next_offset = self->size->value;
  } else next_offset = nnode->image_offset;

  if(pnode) { // Interpolate forward:
    *target_offset_at_point = pnode->target_offset  +             \
      image_period_offset - pnode->image_offset  +                \
      period_number * self->target_period->value;

    *available_to_read = next_offset - readptr;
    if(target_idx)
      *target_idx = pnode->target_idx;

    return self->targets[pnode->target_idx];
  } else if(nnode) { // Interpolate in reverse
    *target_offset_at_point = nnode->target_offset -                     \
      (nnode->image_offset - image_period_offset) +                      \
      period_number * self->target_period->value;

    *available_to_read = nnode->image_offset - readptr;
    if(target_idx)
      *target_idx = nnode->target_idx;

    return self->targets[nnode->target_idx];
  };

  RaiseError(ERuntimeError, "Map is empty...");
  return NULL;
};

VIRTUAL(MapValue, RDFValue) {
  VMETHOD_BASE(RDFValue, raptor_type) = RAPTOR_IDENTIFIER_TYPE_LITERAL;
  VMETHOD_BASE(RDFValue, dataType) = AFF4_MAP_TEXT;

  VMETHOD_BASE(RDFValue, Con) = MapValue_Con;
  VMETHOD_BASE(RDFValue, decode) = MapValue_decode;
  VMETHOD_BASE(RDFValue, serialise) = MapValue_serialise;
  VMETHOD_BASE(MapValue, add_point) = MapValue_add_point;
  VMETHOD_BASE(MapValue, get_range) = MapValue_get_range;
} END_VIRTUAL

static int MapValueBinary_decode(RDFValue self, char *data, int length, RDFURN subject) {
  MapValue this = (MapValue)self;
  FileLikeObject fd;
  void *ctx = talloc_size(NULL, 1);
  RDFURN segment = new_RDFURN(ctx);
  int i;

  CALL(this->urn, set, subject->value);

  // Load some parameters
  CALL(oracle, resolve_value, this->urn, AFF4_IMAGE_PERIOD,
       (RDFValue)this->image_period);

  CALL(oracle, resolve_value, this->urn, AFF4_TARGET_PERIOD,
       (RDFValue)this->target_period);

  CALL(oracle, resolve_value, this->urn, AFF4_SIZE,
       (RDFValue)this->size);

  // Now load our contents from the stored URNs
  CALL(segment,set, this->urn->value);
  CALL(segment, add, "idx");

  fd = (FileLikeObject)CALL(oracle, open, segment, 'r');
  if(!fd) goto exit;

  {
    char *map = CALL(fd,get_data);
    char *x=map;

    // Skip to the next null termination
    while(x - map < fd->size->value && *x) x++;
    if(x!=map) {
      this->targets = talloc_realloc_size(self, this->targets,
                                          (this->number_of_urns + 1) *  \
                                          sizeof(*this->targets));
      this->targets[this->number_of_urns] = new_RDFURN(self);
      CALL(this->targets[this->number_of_urns], set, map);

      this->number_of_urns ++ ;
      x++;
    };
  };

  CALL(oracle, cache_return, (AFFObject)fd);

  // Now load our contents from the stored URNs
  CALL(segment,set, this->urn->value);
  CALL(segment, add, "map");

  // Now do the map segment
  fd = (FileLikeObject)CALL(oracle, open, segment, 'r');
  if(!fd) goto exit;

  this->number_of_points = fd->size->value / sizeof(struct map_point);

  for(i=this->number_of_points; i>0; i--) {
    // Clump up big reads for efficiency
    struct map_point buff[BUFF_SIZE];
    int res = CALL(fd, read, (char *)buff, sizeof(buff));
    int j;

    if(res < 0) break;

    res /= sizeof(struct map_point);

    for(j=0;j<res; j++) {
      map_point_node_t *node = talloc(this, map_point_node_t);

      node->image_offset = ntohll(buff[j].image_offset);
      node->target_offset = ntohll(buff[j].target_offset);
      node->target_idx = ntohll(buff[j].target_index);

      tree_insert(&this->tree, node);
      i--;
    };
  };
  CALL(oracle, cache_return, (AFFObject)fd);

 exit:
  talloc_free(ctx);
  return 1;
};

static map_point_node_t *binary_map_iterate_cb(map_point_tree_t *tree,
                                       map_point_node_t *node,
                                       void *data) {
  FileLikeObject fd = (FileLikeObject)data;
  struct map_point point;

  point.image_offset = htonll(node->image_offset);
  point.target_offset = htonll(node->target_offset);
  point.target_index = htonl(node->target_idx);

  CALL(fd, write, (char *)&point, sizeof(point));

  return NULL;
};

static char *MapValueBinary_serialise(RDFValue self, RDFURN subject) {
  MapValue this = (MapValue)self;
  RDFURN stored = new_RDFURN(self);
  RDFURN segment = new_RDFURN(stored);
  RDFURN index_segment = new_RDFURN(stored);
  int i;
  AFF4Volume volume;
  FileLikeObject fd;

  // Do nothing if there are no points
  if(this->number_of_points == 0)
    goto exit;

  CALL(segment, set, this->urn->value);
  CALL(segment, add, "map");

  CALL(index_segment, set, this->urn->value);
  CALL(index_segment, add, "idx");

  // Check if the map segment already exists - if it does we dont
  // bother re-saving it.
  fd = (FileLikeObject)CALL(oracle, open, segment, 'r');
  if(fd) {
    CALL((AFFObject)fd, cache_return);
    goto exit;
  };

  // Work out where our map object is stored:
  if(!CALL(oracle, resolve_value, this->urn, AFF4_STORED, (RDFValue)stored)) {
    RaiseError(ERuntimeError, "No storage for map object");
    goto error;
  };

  volume = (AFF4Volume)CALL(oracle, open, stored, 'w');
  if(!volume) goto error;

  // Is it actually a volume?
  if(!ISSUBCLASS(volume, AFF4Volume)) {
    RaiseError(ERuntimeError, "URN %s is not a volume!!!", STRING_URNOF(volume));
    CALL((AFFObject)volume, cache_return);
    goto error;
  };

  // First dump the index
  fd = CALL(volume, open_member, index_segment->value, 'w', ZIP_DEFLATE);
  if(!fd) {
    goto error;
  };
  for(i=0; i < this->number_of_urns; i++) {
    CALL(fd, write, ZSTRING(this->targets[i]->value));
  };
  CALL((AFFObject)fd, close);

  // Now the data segment
  fd = CALL(volume, open_member, segment->value, 'w', ZIP_DEFLATE);
  if(!fd) {
    goto error;
  };

  tree_iter(&this->tree, NULL, binary_map_iterate_cb, (void *)fd);
  CALL((AFFObject)fd, close);

  // Done with our volume now
  CALL((AFFObject)volume, cache_return);

  // Now store various map parameters
  if(this->image_period->value != -1) {
    CALL(oracle, set_value, this->urn, AFF4_IMAGE_PERIOD, (RDFValue)this->image_period,0);
    CALL(oracle, set_value, this->urn, AFF4_TARGET_PERIOD, (RDFValue)this->target_period,0);
  };

  CALL(oracle, set_value, this->urn, AFF4_SIZE, (RDFValue)this->size,0);

 exit:
  return talloc_strdup(self, "/map");

 error:
  if(fd)
    talloc_free(fd);
  talloc_free(stored);
  return NULL;
};

VIRTUAL(MapValueBinary, MapValue) {
  VMETHOD_BASE(RDFValue, dataType) = AFF4_MAP_BINARY;

  VMETHOD_BASE(RDFValue, serialise) = MapValueBinary_serialise;
  VMETHOD_BASE(RDFValue, decode) = MapValueBinary_decode;
} END_VIRTUAL

static map_point_node_t *inline_map_iterate_cb(map_point_tree_t *tree,
                                               map_point_node_t *node,
                                               void *data) {
  MapValueInline self = (MapValueInline)data;
  int available_to_write =  BUFF_SIZE - self->i -1;
  uint32_t idx = min(self->super.number_of_points, node->target_idx);

  int need_to_write = snprintf(self->buffer + self->i, available_to_write,
                               "%lld,%lld,%s|", node->image_offset,
                               node->target_offset, tree->value->targets[idx]->value);

  self->i += need_to_write;

  self->i = min(self->i, BUFF_SIZE);
  return NULL;
};

static char *MapValueInline_serialise(RDFValue self, RDFURN subject) {
  MapValueInline this = (MapValueInline)self;

  this->buffer = talloc_zero_size(NULL, BUFF_SIZE);
  this->i = 0;

  // Now save the map to the segment - Iterating over the treap in
  // forward order will produce a sorted array.
  tree_iter(&((MapValue)this)->tree, NULL, inline_map_iterate_cb, (void *)self);

  return this->buffer;
};

static int MapValueInline_decode(RDFValue self, char *data, int length, RDFURN subject) {
  MapValue this = (MapValue)self;

  CALL(this->urn, set, subject->value);

  // Load some parameters
  CALL(oracle, resolve_value, this->urn, AFF4_IMAGE_PERIOD,
       (RDFValue)this->image_period);

  CALL(oracle, resolve_value, this->urn, AFF4_TARGET_PERIOD,
       (RDFValue)this->target_period);

  CALL(oracle, resolve_value, this->urn, AFF4_SIZE,
       (RDFValue)this->size);

  {
    char *map = data;
    char *x=map, *y;
    struct map_point point;
    char target[1024];

    while(strlen(x)>0) {
      // Look for the end of line and null terminate it:
      y= x + strcspn(x, "|");
      *y=0;
      if(sscanf(x,"%lld,%lld,%1000s", &point.image_offset,
                &point.target_offset, target)==3) {
        CALL(this, add_point, point.image_offset,
             point.target_offset, target);
      };
      x=y+1;
    };
  };

  return 1;
};


VIRTUAL(MapValueInline, MapValue) {
  VMETHOD_BASE(RDFValue, dataType) = AFF4_MAP_INLINE;

  VMETHOD_BASE(RDFValue, serialise) = MapValueInline_serialise;
  VMETHOD_BASE(RDFValue, decode) = MapValueInline_decode;
} END_VIRTUAL


/*** This is the implementation of the MapDriver */
static AFFObject MapDriver_Con(AFFObject self, RDFURN uri, char mode){ 
  MapDriver this = (MapDriver)self;

  // Try to parse existing map object
  if(uri) {
    URNOF(self) = CALL(uri, copy, self);

    this->stored = new_RDFURN(self);
    this->target_urn = new_RDFURN(self);
    this->dirty = new_XSDInteger(self);

    // Check that we have a stored property
    if(!CALL(oracle, resolve_value, URNOF(self), AFF4_STORED,
	     (RDFValue)this->stored)) {
      RaiseError(ERuntimeError, "Map object does not have a stored attribute?");
      goto error;
    };

    CALL(oracle, set_value, URNOF(self), AFF4_TYPE, rdfvalue_from_urn(self, AFF4_MAP),0);
    CALL(oracle, add_value, this->stored, AFF4_VOLATILE_CONTAINS, (RDFValue)uri,0);

    // Only update the timestamp if we created a new map
    if(mode=='w') {
      XSDDatetime time = new_XSDDateTime(this);

      this->dirty->value = DIRTY_STATE_NEED_TO_CLOSE;
      CALL(oracle, set_value, URNOF(self), AFF4_TIMESTAMP, (RDFValue)time,0);
      CALL(oracle, set_value, URNOF(self), AFF4_VOLATILE_DIRTY, (RDFValue)this->dirty,0);
      // Make sure that our containing volume becomes dirty so we get
      // written to disk - note that we may not write any segments for
      // a map at all, so we need to explicitely make our volume dirty.
      CALL(oracle, set_value, this->stored, AFF4_VOLATILE_DIRTY, (RDFValue)this->dirty,0);

      // Make a new map object
      if(!this->map) {
        this->map = (MapValue)CALL(oracle, new_rdfvalue, self, AFF4_MAP_INLINE);
      };

      // Make sure the map belongs to us
      CALL(this->map->urn, set, uri->value);
    } else {
      // We try to load the map from the resolver - because we dont
      // know the exact implementation we ask the resolver to allocate
      // it for us.
      this->map = (MapValue)CALL(oracle, resolve_alloc, self, uri, AFF4_MAP_DATA);

      if(!this->map) {
        RaiseError(EIOError, "Unable to open map object");
        goto error;
      };

      if(!ISSUBCLASS(this->map, MapValue)) {
        RaiseError(ERuntimeError, "Got unexpected value for map attribute '%s', needed %s",
                   ((RDFValue)this->map)->dataType, ((RDFValue)&__MapValue)->dataType);
        goto error;
      };
    };

    // We just share the map's size.
    ((FileLikeObject)self)->size = this->map->size;

    // Ignore it if we cant open the map_urn
    ClearError();
  } else {
    self = SUPER(AFFObject, FileLikeObject, Con, uri, mode);
  };

  return self;

 error:
  talloc_free(self);
  return NULL;
};

static void MapDriver_add(MapDriver self, uint64_t image_offset, uint64_t target_offset,
			  char *target) {
  MapDriver this=self;

  CALL(this->map, add_point, image_offset, target_offset, target);

};

static void MapDriver_write_from(MapDriver self, RDFURN target, uint64_t target_offset, uint64_t target_length) {
  FileLikeObject this = (FileLikeObject)self;

  CALL(self->map, add_point, this->readptr, target_offset, target->value);
  this->readptr += target_length;
  this->size->value = max(this->size->value, this->readptr);
};


static int MapDriver_close(FileLikeObject self) {
  MapDriver this = (MapDriver)self;

  // We want the storage volume to be dirty while we write ourselves
  // into it - this is a sanity check which could happen if the user
  // closed the containing volume before they closed the map:
  if(CALL(oracle, resolve_value, this->stored, AFF4_VOLATILE_DIRTY,
          (RDFValue)this->dirty) &&
     this->dirty->value != DIRTY_STATE_NEED_TO_CLOSE) {
    RaiseError(ERuntimeError, "Storage volume is closed with closing map %s",
               STRING_URNOF(self));
    goto error;
  };

  // We choose the most efficient map implementation depending on the
  // size of the map:
  if(!this->custom_map) {
    RDFValue map = ((RDFValue)this->map);

    if(this->map->number_of_points < 2) {
      map->serialise = MapValueInline_serialise;
      map->dataType = AFF4_MAP_INLINE;
      map->id = ((RDFValue)&__MapValueInline)->id;

    } else if(this->map->number_of_points > 10) {
      map->serialise  = MapValueBinary_serialise;
      map->dataType = AFF4_MAP_BINARY;
      map->id = ((RDFValue)&__MapValueBinary)->id;

    } else {
      map->serialise = MapValue_serialise;
      map->dataType = AFF4_MAP_TEXT;
      map->id = ((RDFValue)&__MapValue)->id;
    };
  };

  // Write the map to the stream:
  CALL(oracle, set_value, URNOF(self), AFF4_MAP_DATA, (RDFValue)this->map,0);

  // We are not dirty any more:
  this->dirty->value = DIRTY_STATE_ALREADY_LOADED;
  CALL(oracle, set_value, URNOF(self), AFF4_VOLATILE_DIRTY,
       (RDFValue)this->dirty,0);

  // Done
  CALL(oracle, set_value, URNOF(self), AFF4_VOLATILE_SIZE, (RDFValue)self->size,0);
  SUPER(AFFObject, FileLikeObject, close);
  return 1;

 error:
  PUSH_ERROR_STATE;
  SUPER(AFFObject, FileLikeObject, close);
  POP_ERROR_STATE;

  return 0;
};

// Read as much as possible and return how much was read
static int MapDriver_partial_read(FileLikeObject self, char *buffer, \
				  unsigned long int length) {
  MapDriver this = (MapDriver)self;
  FileLikeObject target_fd;
  int read_bytes;
  uint64_t target_offset_at_point, available_to_read;
  RDFURN target =  CALL(this->map, get_range, self->readptr,
                        &target_offset_at_point, &available_to_read, NULL);

  if(!target) {
    ClearError();
    return 0;
  };

  // Clamp the available_to_read to the length requested
  available_to_read = min(available_to_read, length);

  // Now do the read:
  target_fd = (FileLikeObject)CALL(oracle, open,  target, 'r');
  if(!target_fd) return -1;

  CALL(target_fd, seek, target_offset_at_point, SEEK_SET);
  read_bytes = CALL(target_fd, read, buffer, available_to_read);

  CALL(oracle, cache_return, (AFFObject)target_fd);

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
        read_length = length;
      } else {
	PrintError();
	return -1;
      };
    };
    i += read_length;
  };

  return i;
};

static void MapDriver_set_data_type(MapDriver self, char *type) {
  MapValue tmp = (MapValue)CALL(oracle, new_rdfvalue, self, type);

  if(tmp) {
    self->map = tmp;
    self->custom_map = 1;
  };
};

static uint64_t MapDriver_seek(FileLikeObject self, int64_t offset, int whence) {
  // FIXME - implement sparse maps through seeking on write
  if(((AFFObject)self)->mode == 'r') {
    return SUPER(FileLikeObject, FileLikeObject, seek, offset, whence);
  } else {
    RaiseError(ERuntimeError, "Seeking maps opened for writing is not supported yet!!!");
    return -1;
  };
};

VIRTUAL(MapDriver, FileLikeObject) {
     VMETHOD_BASE(AFFObject, Con) = MapDriver_Con;
     VMETHOD_BASE(AFFObject, dataType) = AFF4_MAP;

     VMETHOD(set_data_type) = MapDriver_set_data_type;
     VMETHOD(add_point) = MapDriver_add;
     VMETHOD(write_from) = MapDriver_write_from;
     VMETHOD_BASE(FileLikeObject, read) = MapDriver_read;
     VMETHOD_BASE(AFFObject, close) = MapDriver_close;
     VMETHOD_BASE(FileLikeObject, seek) = MapDriver_seek;

     // FIXME pass through to the current target to implement sparse streams
     UNIMPLEMENTED(FileLikeObject, write);
} END_VIRTUAL

void mapdriver_init() {
  INIT_CLASS(MapDriver);
  INIT_CLASS(MapValue);
  INIT_CLASS(MapValueBinary);
  INIT_CLASS(MapValueInline);

  register_type_dispatcher(AFF4_MAP, (AFFObject *)GETCLASS(MapDriver));
  register_rdf_value_class((RDFValue)GETCLASS(MapValue));
  register_rdf_value_class((RDFValue)GETCLASS(MapValueBinary));
  register_rdf_value_class((RDFValue)GETCLASS(MapValueInline));
}
