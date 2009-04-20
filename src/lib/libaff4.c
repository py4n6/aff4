/** Public interface for aff4.
 */
#include "zip.h"
#include "aff4.h"

ZipFile open_volume(char *urn, char mode) {
  ZipFile result=NULL;
  char *filename = normalise_url(urn);

  ClearError();

  // Try to pull it from cache
  {
    char *volume = CALL(oracle, resolve, filename, AFF4_CONTAINS);
    if(volume)
      result = (ZipFile)CALL(oracle, open, NULL, volume, mode);
  };

  // Nope - we need to make it. Try a directory volume first:
  if(!result) {
    result =(ZipFile)CONSTRUCT(DirVolume, ZipFile, super.Con, NULL, filename, mode);
    if(result) LogWarnings("Loaded directory volume %s", filename);
  };
  // Nope - maybe a ZipFile?
  if(!result) {
    result = CONSTRUCT(ZipFile, ZipFile, Con, NULL, filename, mode);
    if(result) LogWarnings("Loaded zip volume %s", filename);
  };
  PrintError();

  return result;
};

char **aff4_load(char **images) {
  char **i;
  StringIO result;

  AFF2_Init();

  result = CONSTRUCT(StringIO, StringIO, Con, NULL);

  for(i=images; *i; i++) {
    ZipFile tmp = open_volume(*i, 'r');
    if(tmp) {
      char *urn = talloc_strdup(result->data, URNOF(tmp));
      CALL(result, write, (char *)&urn, sizeof(urn));
      CALL(oracle, cache_return,(AFFObject)tmp);
    };
  };

  if(result->size >0) {
    i=NULL;
    CALL(result, write, (char *)&i, sizeof(i));
    return result->data;
  } else {
    return NULL;
  };
};

/** Opens an AFF4 file for reading */
AFF4_HANDLE aff4_open(char **images) {
  // The opaque handle we actually return is a FileLikeObject.
  FileLikeObject result=NULL;
  char **i;
  // By default we try to open the stream named default
  char *stream="default";

  AFF2_Init();

  for(i=images; *i; i++) {
    // We try to load each image as a URL in itself:
    FileLikeObject fd= (FileLikeObject)CALL(oracle, open, NULL, 
					    normalise_url(*i), 'r');
    ZipFile tmp;

    // This is actually a stream they specified
    if(fd && ISSUBCLASS(fd, FileLikeObject)) {
      stream = URNOF(fd);
    };

    if(fd) {
      CALL(oracle, cache_return, (AFFObject)fd);
    };
    
    tmp = open_volume(*i, 'r');
    if(tmp) {
      CALL(oracle, cache_return, (AFFObject)tmp);
    } else {
      // Its potentially a stream name:
      stream = *i;
    };
  };

  // Ok so now we loaded all volumes, we try to open a stream within
  // them:
  // Is the stream a fully qualified name?
  if(!strstr(stream, FQN)) {
    // Maybe the stream is specified relative to one of the above
    // volumes?
    LogWarnings("Trying to open stream %s relative to all volumes", stream);
    for(i=images; *i; i++) {
      char buffer[BUFF_SIZE];
      ZipFile tmp = open_volume(*i, 'r');

      if(tmp) {
	snprintf(buffer, BUFF_SIZE, "%s/%s", URNOF(tmp), stream);
	LogWarnings("Trying %s", buffer);
	CALL(oracle, cache_return, (AFFObject)tmp);

	result = (FileLikeObject)CALL(oracle, open, NULL, buffer, 'r');
	if(result) {
	  break;
	};
      };
    };
  } else {
    result = (FileLikeObject)CALL(oracle, open, NULL, stream, 'r');
  };


  if(result) {
    LogWarnings("Using %s as the stream name", URNOF(result));
  } else {
    LogWarnings("No suitable stream found");
  };

  return result;
};

void aff4_seek(AFF4_HANDLE self, uint64_t offset, int whence) {
  FileLikeObject result = (FileLikeObject)self;

  if(result) {
    CALL(result, seek, offset, whence);
  };
};

uint64_t aff4_tell(AFF4_HANDLE self) {
  FileLikeObject result = (FileLikeObject)self;

  return CALL(result, tell);
};

int aff4_read(AFF4_HANDLE self, char *buf, int len) {
  FileLikeObject result = (FileLikeObject)self;

  return CALL(result, read, buf, len);
};

void aff4_close(AFF4_HANDLE self) {
  // Just give the handle back to the oracle so it may be reused if
  // needed.
  if(self)
    CALL(oracle, cache_return, (AFFObject)self);
};

struct aff4_tripple **aff4_query(AFF4_HANDLE self, char *urn, 
				char *attribute, char *value) {
  Cache i,j;
  StringIO result = CONSTRUCT(StringIO, StringIO, Con, self);
  struct aff4_tripple **final_result=NULL;
  struct aff4_tripple *tmp;

  list_for_each_entry(i, &oracle->urn->cache_list, cache_list) {
    char *oracle_urn = (char *)i->key;
    Cache attributes = i->data;
    if(urn && !startswith(oracle_urn, urn)) continue;

    list_for_each_entry(j, &attributes->cache_list, cache_list) {
      char *oracle_attribute = (char *)j->key;
      char *oracle_value = (char *)j->data;

      if(attribute && !startswith(oracle_attribute, attribute)) continue;
      if(value && !startswith(oracle_value, value)) continue;

      // Add the match to the result
      tmp = talloc(result->data, struct aff4_tripple);
      tmp->urn = talloc_strdup(tmp, oracle_urn); 
      tmp->attribute = talloc_strdup(tmp, oracle_attribute); 
      tmp->value = talloc_strdup(tmp, oracle_value); 
      CALL(result, write, (char *)&tmp, sizeof(tmp));
    };
  };

  // NULL terminate the result
  CALL(result, write, (char *)&final_result, sizeof(&final_result));
  final_result = (struct aff4_tripple **)result->data;
  talloc_steal(self, final_result);
  talloc_free(result);

  return final_result;
};

/** Loads certificate cert and creates a new identify. All segments
    written from now on will be signed using the identity.

    Note that we use the oracle to load the certificate files into
    memory for openssl - this allows the certs to be stored as URNs
    anywhere (on http:// URIs or inside the volume itself).
*/
void add_identity(char *key_file, char *cert) {
  Identity person;

  person = CONSTRUCT(Identity, Identity, Con, NULL, cert, key_file, 'w');
  
  if(!person) {
    PrintError();
      exit(-1);
  };
};
