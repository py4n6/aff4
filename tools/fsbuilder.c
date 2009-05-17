/** This is a quick program which reads a stream and creates a set of
    map streams for each file in the stream. We use sleuthkit to dump  
    out the block allocations for each file. If we cant find the block
    allocation, (e.g. in NTFS compressed or resident files) we can
    just copy them.
*/
#include "config.h"
#include "common.h"
#include <tsk3/libtsk.h>
#include "aff4.h"
#include "talloc.h"
#include "zip.h"

ZipFile create_volume(char *driver) {
  if(!strcmp(driver, AFF4_ZIP_VOLUME)) {
    return (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  } else if(!strcmp(driver, AFF4_DIRECTORY_VOLUME)) {
    return (ZipFile)CALL(oracle, create, (AFFObject *)&__DirVolume);
  } else {
    RaiseError(ERuntimeError, "Unknown driver %s", driver);
    return NULL;
  };
};

/*** This is a special TSK_IMG_INFO which handles AFF4 streams */
typedef struct {
  TSK_IMG_INFO img_info;
  AFF4_HANDLE handle;
} IMG_AFF4_INFO;

static ssize_t
tsk_aff4_read(TSK_IMG_INFO * img_info, TSK_OFF_T offset, char *buf,
	      size_t len)
{
  IMG_AFF4_INFO *self=(IMG_AFF4_INFO *)img_info;

  aff4_seek(self->handle, offset, 0);
  return aff4_read(self->handle, buf, len);
};

static void
tsk_aff4_close(TSK_IMG_INFO * img_info)
{
    IMG_AFF4_INFO *self = (IMG_AFF4_INFO *) img_info;

    aff4_close(self->handle);
    free(self);
};

TSK_IMG_INFO *tsk_aff4_open(int count, char **filenames) {
  IMG_AFF4_INFO *result;
  // Make sure the library is initialised:
  result= talloc(NULL, IMG_AFF4_INFO);

  if(!result) 
    goto error1;
  
  result->handle = aff4_open((char **)filenames);
  if(!result->handle) goto error;

  aff4_seek(result->handle, 0, 2);
  result->img_info.size = aff4_tell(result->handle);
  result->img_info.itype = TSK_IMG_TYPE_AFF4;
  result->img_info.read = tsk_aff4_read;
  result->img_info.close = tsk_aff4_close; 

  return (TSK_IMG_INFO *)result;

 error:
  talloc_free(result);
 error1:
  return NULL;
};

/** \internal 
* Structure to store data for callbacks.
*/
typedef struct {
    /* Time skew of the system in seconds */
    int32_t sec_skew;

    /*directory prefix for printing mactime output */
    char *macpre;
    int flags;
  // This is the volume we will be writing on
  char *volume_urn;

  // This is the stream we will be targeting
  char *target_urn;
  MapDriver map;
} FLS_DATA;


int print_addr_act(TSK_FS_FILE *fs_file, TSK_OFF_T offset, 
		   TSK_DADDR_T addr, char *buff, 
		   size_t size, TSK_FS_BLOCK_FLAG_ENUM flags, void *ptr) {
  FLS_DATA *self= (FLS_DATA *)ptr;
  MapDriver m = self->map;

  CALL(self->map, add, offset, addr * fs_file->fs_info->block_size, self->target_urn);

  return TSK_WALK_CONT;
};

static void
printit(TSK_FS_FILE * fs_file, const char *a_path,
    const TSK_FS_ATTR * fs_attr, FLS_DATA * fls_data)
{
    unsigned int i;
    char *path_name;
    //    char *link_urn = fully_qualified_name("1", URNOF(m));
    char *link_urn = fls_data->target_urn;

    path_name = talloc_asprintf(NULL, "%s%s/%s", fls_data->volume_urn, 
				a_path, 
				fs_file->name->name);

    printf("%s!\n", path_name);
    if(fs_file->meta->type != TSK_FS_META_TYPE_REG) return;

    fls_data->map = (MapDriver)CALL(oracle, create, (AFFObject *)&__MapDriver);

    URNOF(fls_data->map) = path_name;

    // Where do we want to store it? Which stream do we want to target?
    CALL(oracle, set, URNOF(fls_data->map), AFF4_STORED, fls_data->volume_urn);

    // How big are we?
    CALL(oracle, set, URNOF(fls_data->map), AFF4_SIZE, from_int(fs_file->meta->size));

    if(!CALL((AFFObject)fls_data->map, finish))
      return;

    // Now build a link to the target - this is done in order to make
    // the properties file smaller (otherwise it would have the full
    // target urn on each line.
    //    CALL(oracle, set, link_urn, AFF4_TYPE, AFF4_LINK);
    //    CALL(oracle, set, link_urn, AFF4_TARGET, fls_data->target_urn);

    printf("File name %s\n", path_name);
    talloc_free(path_name);

    if (tsk_fs_file_walk(fs_file, TSK_FS_FILE_WALK_FLAG_AONLY,
			 print_addr_act, (void *)fls_data)) {

    };

    CALL(fls_data->map, save_map);
    CALL((FileLikeObject)fls_data->map, close);
};

static TSK_WALK_RET_ENUM
print_dent_act(TSK_FS_FILE * fs_file, const char *a_path, void *ptr) {
  FLS_DATA *fls_data = (FLS_DATA *) ptr;
  
  /* only print dirs if TSK_FS_FLS_DIR is set and only print everything
  ** else if TSK_FS_FLS_FILE is set (or we aren't sure what it is)
  */
  if (((fls_data->flags & TSK_FS_FLS_DIR) &&
       ((fs_file->meta) &&
	(fs_file->meta->type == TSK_FS_META_TYPE_DIR)))
      || ((fls_data->flags & TSK_FS_FLS_FILE) && (((fs_file->meta)
						   && (fs_file->meta->type != TSK_FS_META_TYPE_DIR))
						  || (!fs_file->meta)))) {
    
    
    /* Make a special case for NTFS so we can identify all of the
     * alternate data streams!
     */
    if ((TSK_FS_TYPE_ISNTFS(fs_file->fs_info->ftype))
	&& (fs_file->meta)) {
      uint8_t printed = 0;
      int i, cnt;
      
      // cycle through the attributes
      cnt = tsk_fs_file_attr_getsize(fs_file);
      for (i = 0; i < cnt; i++) {
	const TSK_FS_ATTR *fs_attr =
	  tsk_fs_file_attr_get_idx(fs_file, i);
	if (!fs_attr)
	  continue;
	
	if (fs_attr->type == TSK_FS_ATTR_TYPE_NTFS_DATA) {
	  printed = 1;
	  
	  if (fs_file->meta->type == TSK_FS_META_TYPE_DIR) {
	    
	    /* we don't want to print the ..:blah stream if
	     * the -a flag was not given
	     */
	    if ((fs_file->name->name[0] == '.')
		&& (fs_file->name->name[1])
		&& (fs_file->name->name[2] == '\0')
		&& ((fls_data->flags & TSK_FS_FLS_DOT) == 0)) {
	      continue;
	    }
	  }
	  
	  printit(fs_file, a_path, fs_attr, fls_data);
	}
	else if (fs_attr->type == TSK_FS_ATTR_TYPE_NTFS_IDXROOT) {
	  printed = 1;
	  
	  /* If it is . or .. only print it if the flags say so,
	   * we continue with other streams though in case the 
	   * directory has a data stream 
	   */
	  if (!((TSK_FS_ISDOT(fs_file->name->name)) &&
		((fls_data->flags & TSK_FS_FLS_DOT) == 0)))
	    printit(fs_file, a_path, fs_attr, fls_data);
	}
      }
      
      /* A user reported that an allocated file had the standard
       * attributes, but no $Data.  We should print something */
      if (printed == 0) {
	printit(fs_file, a_path, NULL, fls_data);
      }
      
    }
    else {
      /* skip it if it is . or .. and we don't want them */
      if (!((TSK_FS_ISDOT(fs_file->name->name))
	    && ((fls_data->flags & TSK_FS_FLS_DOT) == 0)))
	printit(fs_file, a_path, NULL, fls_data);
    }
  }
  return TSK_WALK_CONT;
};


int main(int argc, char **argv)
{
  int c;
  char *output_file = NULL;
  char *stream_name = "default";
  char *driver = AFF4_ZIP_VOLUME;
  int verbose=0;
  char *cert = NULL;
  char *key_file = NULL;
  char *volumes[argc];
  int i=0;
  TSK_IMG_INFO *img;
  TSK_FS_TYPE_ENUM fstype = TSK_FS_TYPE_DETECT;
  TSK_FS_INFO *fs;
  int fls_flags = TSK_FS_FLS_FULL | TSK_FS_FLS_FILE | TSK_FS_FLS_DIR;
  TSK_INUM_T inode = 0;
  int name_flags = TSK_FS_NAME_FLAG_ALLOC | TSK_FS_NAME_FLAG_UNALLOC | TSK_FS_DIR_WALK_FLAG_RECURSE;
  static TSK_TCHAR *macpre = NULL;
  int32_t sec_skew = 0;

  memset(volumes,0, argc);

  //talloc_enable_leak_report_full();

  while (1) {
    int option_index = 0;
    // Note that we use an extension to long_options to allow the
    // helpful descriptions to be included with the long names. This
    // keeps everything well synchronised in the same place.
    static struct option long_options[] = {
      {"help\0"
       "This message", 0, 0, 'h'},
      {"verbose\0"
       "Verbose (can be specified more than once)", 0, 0, 'v'},

      {"output\0"
       "Create the output volume on this file or URL (using webdav)", 1, 0, 'o'},
      {"cert\0"
       "Certificate to use for signing", 1, 0, 'c'},
      {"key\0"
       "Private key in PEM format", 1, 0, 'k'},
      {"fstype\0"
       "filesystem type", 1, 0, 'f'},

      {"inode\0"
       "Inode to export", 1, 0, 'i'},
      {"recurse\0"
       "Recurse into subdirectories", 0, 0, 'r'},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, generate_short_optargs(long_options),
		    long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
    case 'c':
      cert = optarg;
      break;

    case 'k':
      key_file = optarg;
      break;

    case 'o':
      output_file = optarg;
      break;

    case 'i':
    if (tsk_fs_parse_inum(optarg, &inode, NULL, NULL, NULL, NULL)) {
      printf("%s is not an inode\n", optarg);
      goto error;
    };
    break;

    case 'd':
      driver = optarg;
      break;

    case 'r':
      fls_flags |=TSK_FS_DIR_WALK_FLAG_RECURSE;
      break;

    case 'f':
      if (TSTRCMP(OPTARG, _TSK_T("list")) == 0) {
	tsk_fs_type_print(stderr);
	exit(1);
      }
      fstype = tsk_fs_type_toid(OPTARG);
      if (fstype == TSK_FS_TYPE_UNSUPP) {
	TFPRINTF(stderr,
		 _TSK_T("Unsupported file system type: %s\n"), OPTARG);
	exit(-1);
      }
      break;

    case 's':
      stream_name = optarg;
      break;

    case 'v':
      verbose++;
      break;

    case '?':
    case 'h':
      printf("\n%s [options] [volumes] source \n\nWrites filesystem as AFF4 map stream.\n\n", argv[0]);
      print_help(long_options);
      exit(0);

    default:
      printf("?? getopt returned character code 0%o ??\n", c);
    }
  }

  if(!output_file) {
    printf("You must specify an output file with --output\n");
    exit(-1);
  };
  
  if(cert || key_file)
    add_identity(key_file, cert);

  i=0;
  while (optind < argc ) {
    volumes[i]=argv[optind++];
    i++;
  };

  volumes[i++]=stream_name;
  volumes[i]=0;

  img = tsk_aff4_open(i, volumes);
  if(!img) {
    tsk_error_print(stderr);
    goto error;
  }

  fs = tsk_fs_open_img(img, 0, fstype);
  if(!fs) {
    tsk_error_print(stderr);
  };

  if(!inode)
    inode = fs->root_inum;
 
  {
    FLS_DATA data;
    ZipFile output = create_volume(driver);
    
    CALL((AFFObject)output, set_property, AFF4_STORED, normalise_url(output_file));
    if(!CALL((AFFObject)output, finish))
      goto error;

    data.flags = fls_flags;
    data.sec_skew = sec_skew;
    data.macpre = macpre;
    data.volume_urn = talloc_strdup(NULL,URNOF(output));
    data.target_urn = URNOF(((IMG_AFF4_INFO *)img)->handle);

    CALL(oracle, cache_return, (AFFObject)output);

    if(output) {
      fls_flags = 7;
      tsk_fs_dir_walk(fs, inode, fls_flags, print_dent_act, &data);

      output = (ZipFile)CALL(oracle, open, data.volume_urn, 'w');
      if(output)
	CALL(output, close);

    };
  };

  PrintError();
  exit(EXIT_SUCCESS);
error:
  exit(-1);
}
