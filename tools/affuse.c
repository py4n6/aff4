#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "misc.h"
#include "aff4.h"
#include "zip.h"

#define USE_FUSE 1

#ifdef USE_FUSE
#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <fcntl.h>
#include <libgen.h>
#include <time.h>

struct stream_info {
  char *urn;
  // This is a possibly shortened urn relative to the volume - its
  // basiclly how the stream shows up in fuse.
  char *path_name;
  uint64_t size;
  time_t mtime;
} *streams;

static char **volumes=NULL;

static int
affuse_getattr(const char *path, struct stat *stbuf)
{
    int res = 0;
    struct stream_info *i;
    char *filename = unescape_filename(NULL, path+1);
    int is_a_stream=0;

    // Truncate the extension
    if(endswith(filename, ".dd")) {
      filename[strlen(filename)-3]=0;
      is_a_stream = 1;
    };

    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 4;
	goto exit;
    };

    if(is_a_stream) {
      for(i=streams; i->urn; i++) {
	if(!strcmp(i->path_name, filename)) {
	  stbuf->st_mode = S_IFREG | 0444;
	  stbuf->st_nlink = 1;
	  stbuf->st_mtime = i->mtime;
	  stbuf->st_atime = i->mtime;
	  stbuf->st_ctime = i->mtime;
	  
	  // What it its size?
	  stbuf->st_size = i->size;
	  goto exit;
	};
      };

      // Its a directory
    } else {
      int i;

      // Is this subdirectory of a stream?
      for(i=0; streams[i].urn; i++) {
	if(startswith(streams[i].path_name, filename) && streams[i].path_name[strlen(filename)]=='/') {
	  stbuf->st_mode = S_IFDIR | 0755;
	  stbuf->st_nlink = 2;
	  stbuf->st_size = 1;
	  stbuf->st_mtime = streams[i].mtime;
	  stbuf->st_atime = streams[i].mtime;
	  stbuf->st_ctime = streams[i].mtime;
	  
	  goto exit;
	};
      };
    };

    talloc_free(filename);
    return -ENOENT;

 exit:
    talloc_free(filename);
    return res;
}

static int
affuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi)
{
  int i;
  Cache dict = CONSTRUCT(Cache, Cache, Con, NULL, HASH_TABLE_SIZE, 0);
  Cache dict_iter;
  char *filename = talloc_strdup(dict, unescape_filename(dict, path+1));

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);

  for(i=0; streams[i].urn; i++) {
    char buffer[BUFF_SIZE];
    char *name=NULL;
    char *j;

    // Only match the objects in this path
    if(!startswith(streams[i].path_name, filename)) continue;
    
    j=streams[i].path_name + strlen(filename);
    // Shift the buffer until we have no leading /
    do {
      strncpy(buffer, j, BUFF_SIZE);
      j++;
    } while(buffer[0]=='/');

    if(strlen(buffer)==0) continue;

    // Find the next path seperator
    for(j=buffer;j<buffer + BUFF_SIZE;j++) {
      if(*j=='/') {
	*j=0;
	name = talloc_strdup(dict, buffer);
	break;
      };
      if(*j==0) {
	// Its a complete image we add a .dd extension to seperate it
	// from directories.
	name = talloc_asprintf(dict, "%s.dd", buffer);
	break;
      };
    };

    // Store it in the cache
    if(name && !CALL(dict, get, name)) {
      CALL(dict, put, name, talloc_strdup(dict, ""),0);
    };
  };
       
  // Now go over all the cache and fill it into the directory
  list_for_each_entry(dict_iter, &dict->cache_list, cache_list) {
    char *name = (char *)dict_iter->key;
    filler(buf, escape_filename(dict, name), NULL, 0);
  };

  talloc_free(dict);

  return 0;
}

static int
affuse_open(const char *path, struct fuse_file_info *fi)
{
  char *buffer=unescape_filename(NULL, path+1);
  int i;

  if(!endswith(buffer, ".dd")) {
    talloc_free(buffer);
    return -ENOENT;
  };

  if((fi->flags & 3) != O_RDONLY) {
    talloc_free(buffer);
    return -EACCES;
  };

  // Now drop the .dd extension
  buffer[strlen(buffer)-3]=0;

  // Is this a valid stream?
  for(i=0; streams[i].urn; i++) {
    if(!strcmp(streams[i].path_name, buffer)) {
      fi->fh = (uint64_t)i;
      talloc_free(buffer);
      return 0;
    };
  };

  talloc_free(buffer);
  return -ENOENT;
}

static int
affuse_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
    int res = 0;
    char *urn = streams[fi->fh].urn;
    FileLikeObject fh;

    fh = (FileLikeObject)CALL(oracle, open, urn, 'r');

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
    fargv = talloc_array(volume_names, char *, argc);
    fargv[0] = argv[0];
    fargv[1] = argv[argc - 1];
    fargc = 2;
    for(i=1; i<argc-1;i++) {
      if (strcmp(argv[i], "-h") == 0 ||
	  strcmp(argv[i], "--help") == 0 ) {
	usage();
	talloc_free(volume_names);
	exit(EXIT_SUCCESS);
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
    AFF2_Init();
      
    // Load all the volumes into the resolver
    volumes = aff4_load(volume_names);
    if(!volumes) goto error;

    // We pull out all urns with a stream interface:
   { 
     struct aff4_tripple **query = aff4_query(NULL,
					      NULL, // All URNs should match
					      AFF4_INTERFACE,  // With an interface attribute
					      AFF4_STREAM);  // Of stream.
     struct stream_info info;
     StringIO result = CONSTRUCT(StringIO, StringIO, Con, NULL);
     char **j;
     struct aff4_tripple **i;

     // Fix up the stream names if they are relative to any volume
     for(i=query; *i; i++) {
       info.urn = (*i)->urn;
       info.size = parse_int(CALL(oracle, resolve, result, info.urn, AFF4_SIZE));
       info.mtime = parse_int(CALL(oracle, resolve, result, info.urn, AFF4_TIMESTAMP));
       
       // If a urn is relative to any of our volumes we merge it:
       for(j=volumes; *j; j++) {
	 if(startswith(info.urn, *j)) {
	   // We store the relative URN in the value member
	   info.path_name = info.urn + strlen(*j)+1;
	   break;
	 } else {
	   info.path_name = talloc_asprintf(NULL, "__URN__/%s", info.urn);
	  };
	}; 
       // Now we write the info into an array
       CALL(result, write, (char *)&info, sizeof(info));
     };
     memset(&info, 0, sizeof(info));
     CALL(result, write, (char *)&info, sizeof(info));

     streams = (struct stream_info *)result->data;
    };

    return fuse_main(fargc, fargv, &affuse_oper, NULL);

 error:
    return 0;
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
