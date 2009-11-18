/** Public interface for aff4.
 */
#include "zip.h"
#include "aff4.h"
#include <libgen.h>

ZipFile open_volume(char *filename, char mode) {
  ZipFile result=NULL;
  RDFURN file_urn = new_RDFURN(NULL);
  RDFURN volume_urn = new_RDFURN(file_urn);
  struct dispatch_t *i=volume_handlers;
  URLParse parser = CONSTRUCT(URLParse, URLParse, Con, file_urn, filename);

  // Normalise the URL by parsing it and canonicalising it
  CALL(file_urn, set, parser->string(parser, file_urn));

  ClearError();
  // First see if we can even open the file.
  {
    AFFObject fd = CALL(oracle, open, file_urn, mode);
    if(!fd) return NULL;
    CALL(oracle, cache_return, fd);
  };

  // Is there a known volume contained in this file?
  if(CALL(oracle, resolve2, file_urn, AFF4_CONTAINS, (RDFValue)volume_urn)) {
    result = (ZipFile)CALL(oracle, open, volume_urn, mode);
  };

  // Go through all the volume handlers trying to create a new
  // volume, until one works
  while(!result && i->class_ptr) {
    result = (ZipFile)CONSTRUCT_FROM_REFERENCE(((AFFObject)i->class_ptr), 
					       Con, NULL, NULL, mode);
    if(result) {
      LogWarnings("Loaded %s volume %s", i->type, filename);
      CALL(oracle, set_value, URNOF(result), AFF4_STORED, (RDFValue)file_urn);
      result = (ZipFile)CALL((AFFObject)result, finish);
      break;
    };
    i++;
  };
  PrintError();

  // Check for autoload in this volume
#if 0
  if(result && mode=='r') {
    char *autoload = (char *)resolver_get_with_default(oracle, CONFIGURATION_NS,
						       CONFIG_AUTOLOAD, "1",
						       RESOLVER_DATA_UINT64);
    char base_path[BUFF_SIZE]; 
    char *dirpath;

    strncpy(base_path, urn, BUFF_SIZE);
    dirpath = dirname(base_path);

    if(!strcmp(autoload, "1")) {
      // Get a list of autoload volumes:
      struct aff4_tripple **result_set = aff4_query(oracle, URNOF(result), AFF4_AUTOLOAD, NULL);
      int i;
      char *result_urn = talloc_strdup(result_set, URNOF(result));

      CALL(oracle, cache_return, (AFFObject)result);

      // Open them all and return to cache
      for(i=0; result_set[i]; i++) {
	// Note that autoloads are specified relative to the current
	// urn path, or in a fully qualified way:
	ZipFile fd;
	char *path = fully_qualified_name(NULL, result_set[i]->value, dirpath);
	
	LogWarnings("Autoloading %s", path);
	fd = open_volume(path, 'r');
	if(fd) {
	  CALL(oracle, cache_return, (AFFObject)fd);
	};
      };

      // Get it back
      result = (ZipFile)CALL(oracle, open, result_urn, mode);

      // Remove autoloads from our resolver
      CALL(oracle, del, URNOF(result), AFF4_AUTOLOAD);
      // Done.
      talloc_free(result_set);
    };
  };
#endif

  talloc_free(file_urn);
  return result;
};

char **aff4_load(char **images) {
  char **i;
  StringIO result;

  AFF4_Init();

  result = CONSTRUCT(StringIO, StringIO, Con, NULL);

  for(i=images; *i; i++) {
    ZipFile tmp = open_volume(*i, 'r');
    if(tmp) {
      // Add the volume URN to the volumes list
      char *urn = talloc_strdup(result->data, URNOF(tmp));
      CALL(result, write, (char *)&urn, sizeof(urn));
      CALL(oracle, cache_return,(AFFObject)tmp);
    };
  };

  if(result->size >0) {
    i=NULL;
    CALL(result, write, (char *)&i, sizeof(i));
    return (char **)(result->data);
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
  char *ctx = talloc_size(NULL, 1);

  AFF4_Init();

  for(i=images; *i; i++) {
    // We try to load each image as a URL in itself:
    ZipFile tmp = open_volume(*i, 'r');
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

	result = (FileLikeObject)CALL(oracle, open, buffer, 'r');
	if(result) {
	  break;
	};
      };
    };
  } else {
    result = (FileLikeObject)CALL(oracle, open, stream, 'r');
  };

  // Lets find the first available stream:
  if(!result) {
    struct aff4_tripple **result_set = aff4_query(oracle, NULL, AFF4_INTERFACE, AFF4_STREAM);
    int i;
    
    for(i=0; result_set[i]; i++) {
      char *type = (char *)CALL(oracle, resolve, ctx, result_set[i]->urn, AFF4_TYPE,
				RESOLVER_DATA_URN);
      if(type && !strcmp(type, AFF4_IMAGE)) {
	result = (FileLikeObject)CALL(oracle, open, 
				      result_set[i]->urn, 'r');
	break;
      };
    };

    if(!result)
      for(i=0; result_set[i]; i++) {
	char *type = (char *)CALL(oracle, resolve, ctx, result_set[i]->urn, AFF4_TYPE,
				  RESOLVER_DATA_URN);
	if(type && !strcmp(type, AFF4_MAP)) {
	  result = (FileLikeObject)CALL(oracle, open, 
					result_set[i]->urn, 'r');
	  break;
	};
      };
  };


  if(result) {
    LogWarnings("Using %s as the stream name", URNOF(result));
  } else {
    LogWarnings("No suitable stream found");
  };

  talloc_free(ctx);
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
/*   Cache i,j; */
/*   StringIO result = CONSTRUCT(StringIO, StringIO, Con, self); */
/*   struct aff4_tripple **final_result=NULL; */
/*   struct aff4_tripple *tmp; */

/*   list_for_each_entry(i, &oracle->urn->cache_list, cache_list) { */
/*     char *oracle_urn = (char *)i->key; */
/*     Cache attributes = i->data; */
/*     if(urn && !startswith(oracle_urn, urn)) continue; */

/*     list_for_each_entry(j, &attributes->cache_list, cache_list) { */
/*       char *oracle_attribute = (char *)j->key; */
/*       char *oracle_value = (char *)j->data; */

/*       if(attribute && !startswith(oracle_attribute, attribute)) continue; */
/*       if(value && !startswith(oracle_value, value)) continue; */

/*       // Add the match to the result */
/*       tmp = talloc(result->data, struct aff4_tripple); */
/*       tmp->urn = talloc_strdup(tmp, oracle_urn);  */
/*       tmp->attribute = talloc_strdup(tmp, oracle_attribute);  */
/*       tmp->value = talloc_strdup(tmp, oracle_value);  */
/*       CALL(result, write, (char *)&tmp, sizeof(tmp)); */
/*     }; */
/*   }; */

/*   // NULL terminate the result */
/*   CALL(result, write, (char *)&final_result, sizeof(&final_result)); */
/*   final_result = (struct aff4_tripple **)result->data; */
/*   talloc_steal(self, final_result); */
/*   talloc_free(result); */

/*   return final_result; */
};

/** Loads certificate cert and creates a new identify. All segments
    written from now on will be signed using the identity.

    Note that we use the oracle to load the certificate files into
    memory for openssl - this allows the certs to be stored as URNs
    anywhere (on http:// URIs or inside the volume itself).
*/
#if 0
void add_identity(char *key_file, char *cert_file) {
  Identity person;
  char *key = talloc_strdup(NULL, normalise_url(key_file));
  char *cert = talloc_strdup(key, normalise_url(cert_file));

  person = CONSTRUCT(Identity, Identity, Con, NULL, cert, key, 'w');
  
  talloc_free(key);
  if(!person) {
    PrintError();
      exit(-1);
  };
};
#endif
