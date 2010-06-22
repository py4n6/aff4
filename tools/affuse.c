#include "aff4.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "misc.h"
#include "aff4.h"

#define USE_FUSE 1

#ifdef USE_FUSE
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <fcntl.h>
#include <libgen.h>
#include <time.h>

extern Resolver oracle;

struct stream_info {
  RDFURN urn;
  XSDInteger size;
  XSDDatetime time;
} *streams;

static int
affuse_getattr(const char *path, struct stat *stbuf) {
    int res = 0;
    struct stream_info *i;
    const char *filename = path + 1;
    char *dirname=NULL;
    int len = strlen(filename);

    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 4;
	goto exit;
    };

    if(filename[len]=='/') {
      dirname = talloc_strdup(NULL, filename);
    } else {
      dirname = talloc_asprintf(NULL, "%s/", filename);
    };

    //Find the filename in our list of streams
    for(i=streams; i->urn; i++) {
      char *stream_urn = i->urn->value + strlen(FQN);

      // Exact match
      if(!strcmp(filename, stream_urn)) {
	  stbuf->st_mode = S_IFREG | 0444;
	  stbuf->st_nlink = 1;
	  stbuf->st_mtime = i->time->value.tv_sec;
	  stbuf->st_atime = stbuf->st_mtime;
	  stbuf->st_ctime = stbuf->st_mtime;
	  stbuf->st_size = i->size->value;
          goto exit;
      } else if(startswith(stream_urn, dirname)) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_size = 1;
        stbuf->st_mtime = i->time->value.tv_sec;
        stbuf->st_atime = stbuf->st_mtime;
        stbuf->st_ctime = stbuf->st_mtime;
        goto exit;
      };
    };

    talloc_free(dirname);
    return -ENOENT;

exit:
    if(dirname) talloc_free(dirname);
    return res;
}

static int
affuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi)
{
  struct stream_info *i;
  int len = strlen(path);
  // This is used to ensure we do not repeat the same directory
  // multiple times.
  Cache cache = CONSTRUCT(Cache, Cache, Con, NULL, 100, 0);

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  for(i=streams; i->urn; i++) {
    char *stream_urn = talloc_strdup(NULL, i->urn->value + strlen(FQN) - 1);

    if(startswith(stream_urn, (char *)path)) {
      char *start,*j = stream_urn + len;

      // Skip any leading /
      while(*j == '/') j++;
      start = j;

      for(; *j; j++) {
        if(*j=='/') {
          *j=0; j++;
          break;
        };
      };

      printf("%s\n", start);
      // Check to see if we already emitted this directory
      if(!CALL(cache, present, ZSTRING_NO_NULL(start))) {
        CALL(cache, put, ZSTRING_NO_NULL(start), NULL);
        filler(buf, start, NULL, 0);
      };

      talloc_free(stream_urn);
    };
  };

  talloc_free(cache);
  return 0;
}

static int
affuse_open(const char *path, struct fuse_file_info *fi)
{
  uint64_t i;

  for(i=0; streams[i].urn; i++) {
    char *stream_urn = streams[i].urn->value + strlen(FQN) - 1;

    // Found it
    if(!strcmp(stream_urn, path)) {

      // Only allow readonly access
      if((fi->flags & 3) != O_RDONLY) {
        return -EACCES;
      };

      // return the position in the list
      fi->fh = i;
      return 0;
    };
  };

  return -ENOENT;
}

static int
affuse_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    int res = 0;
    FileLikeObject fh = (FileLikeObject)CALL(oracle, open,
                                             streams[fi->fh].urn, 'r');

    if(!fh) goto error;

    CALL(fh, seek, (uint64_t)offset, SEEK_SET);
    errno = 0;
    res = CALL(fh, read, (char *)buf, (int)size);

    if (res<0){
	if (errno==0) 
	  errno= -EIO;

	res = -EIO;
    }

    CALL(oracle, cache_return, (AFFObject)fh);
    return res;

 error:
    PrintError();
    return -ENOENT;
}

static struct fuse_operations affuse_oper = {
     .getattr    = affuse_getattr,
     .readdir    = affuse_readdir,
     .open       = affuse_open,
     .read       = affuse_read,
};
 
static void
usage(void)
{
    char *cmdline[] = {"affuse", "-ho"};
    printf("affuse version %s\n", PACKAGE_VERSION);
    printf("Usage: affuse [<FUSE library options>] af_image1 af_image2 mount_point\n");
    /* dirty, just to get current libfuse option list */
    fuse_main(2, cmdline, &affuse_oper, NULL);
    printf("\nUse fusermount -u mount_point, to unmount\n");
}

// Go over all the volumes as stored in the files specified and
// extract all streams.
struct stream_info *populate_streams(char **file_urns, 
                                     int count, int consider_segments) {
  // Somewhere to store the streams on
  StringIO result = CONSTRUCT(StringIO, StringIO, Con, NULL);
  void *ctx = talloc_size(NULL, 1);
  struct stream_info stream;
  RDFURN volume_urn = new_RDFURN(result);
  RDFURN stream_urn = new_RDFURN(result);
  XSDString type = new_XSDString(result);
  int i;
  ZipFile volume;

  for(i=0; i<count; i++) {
    RESOLVER_ITER *iter;

    if(file_urns[i] == NULL) goto exit;

    // Try to load it
    CALL(volume_urn, set, file_urns[i]);
    if(!CALL(oracle, load, volume_urn)) continue;

    // Iterate over all the streams contained here
    iter = CALL(oracle, get_iter, result, volume_urn,
                AFF4_VOLATILE_CONTAINS);

    while(CALL(oracle, iter_next, iter, (RDFValue)stream_urn)) {
      // What type is it? If we dont consider segments and its a
      // segment - skip it.
      if(CALL(oracle, resolve_value, stream_urn, AFF4_TYPE,
              (RDFValue)type) &&
         !consider_segments &&
         !strcmp(type->value, AFF4_SEGMENT))
        continue;

      // Find out more about the stream
      stream.size = new_XSDInteger(ctx);
      CALL(oracle, resolve_value, stream_urn, AFF4_SIZE,
           (RDFValue)stream.size);

      stream.time = new_XSDDateTime(ctx);
      CALL(oracle, resolve_value, stream_urn, AFF4_TIMESTAMP,
           (RDFValue)stream.time);

      // Add the stream
      stream.urn = CALL(stream_urn, copy, ctx);
      CALL(result, write, (char *)&stream, sizeof(stream));
    };
  };

 exit:

  // NULL terminate the list
  stream.urn = NULL;
  CALL(result, write, (char *)&stream, sizeof(stream));

  // Make a fresh copy
  ctx = talloc_realloc_size(ctx, ctx, result->size);
  memcpy(ctx, result->data, result->size);
  //  talloc_free(result);

  return ctx;
};


int main(int argc, char **argv)
{
    char **fargv = NULL;
    int fargc = 0;
    char **volume_names = talloc_array(NULL, char *, argc);
    int i;

    if (argc < 3) {
        usage();
	exit(EXIT_FAILURE);
    }

    /* Prepare fuse args */
    fargv = talloc_array(volume_names, char *, argc + 1);
    fargv[0] = argv[0];
    fargv[1] = argv[argc - 1];
    fargc = 2;
    for(i=1; i<argc-1;i++) {
      if (strcmp(argv[i], "-h") == 0 ||
	  strcmp(argv[i], "--help") == 0 ) {
	usage();
	talloc_free(volume_names);
	exit(EXIT_SUCCESS);
      } else if(!strcmp(argv[i],"-v")) {
        AFF4_DEBUG_LEVEL = 1;
        continue;
      } else if(!strcmp(argv[i],"--")) {
	int j,k;

	for(j=i+1,k=0;j < argc-1; j++,k++) {
	  volume_names[k]=argv[j];
	};

	break;
      };
      fargv[fargc] = argv[i];
      fargc++;
    }
    /* disable multi-threaded operation
     * (we don't know if afflib is thread safe!)
     */
    fargv[fargc] = "-s";
    fargc++;

    // Make sure the library is initialised:
    AFF4_Init();
    oracle = CONSTRUCT(Resolver, Resolver, Con, NULL, 0);
    //oracle = get_oracle();

    streams = populate_streams(volume_names, argc, 0);

    printf("Streams accessible\n---------------------\n\n");
    {
      struct stream_info *i;

      for(i=streams; i->urn; i++) {
        printf("%llu\t%s\n", i->size->value, i->urn->value);
      };
    };

    return fuse_main(fargc, fargv, &affuse_oper, NULL);
}
#else
int main(int argc,char **argv)
{
    fprintf(stderr,"affuse: FUSE support is disabled.\n");
#ifndef linux
    fprintf(stderr,"affuse was compiled on a platform that does not support FUSE\n");
#else
    fprintf(stderr,"affuse was compiled on a Linux system that did not\n");
    fprintf(stderr,"have the FUSE developer libraries installed\n");
#endif
    exit(1);
}
#endif
