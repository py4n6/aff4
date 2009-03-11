#include <getopt.h>
#include "zip.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

void help() {
  printf("aff2imager tool\n\n"
	 "--open, -o \tOpen this file to pre-populate the resolver \n"
	 "           \t(Can be specified multiple times).\n"
	 "--info, -i \tList information about all known objects.\n"
	 );
};

void list_info() {
  char *result =  CALL(oracle, export_all);
  printf("%s", result);

  talloc_free(result);
};

void aff2_open_volume(char *urn) {
  // Try the different volume implementations in turn until one works:
  ZipFile volume;
  char *filename;

  ClearError();
  volume = CONSTRUCT(ZipFile, ZipFile, Con, NULL, urn);
  if(CheckError(ERuntimeError)) {
    ClearError();
    RaiseError(ERuntimeError, "Could not open %s - will try to use file protocol.", urn);
    volume = CONSTRUCT(ZipFile, ZipFile, Con, NULL, talloc_asprintf(NULL, "file://%s",urn));    
  };

  PrintError();

  CALL(oracle, cache_return, (AFFObject)volume);
};

void aff2_print_info() {
  Cache i,j;

  list_for_each_entry(i, &oracle->urn->cache_list, cache_list) {
    char *urn = (char *)i->key;
    Cache attributes = (Cache)i->data;

    printf("\n******** Object %s ***********\n",urn);
    list_for_each_entry(j, &attributes->cache_list, cache_list) {
      printf("\t%s = %s\n", (char *)j->key, (char *)j->data);
    };
  };
};

int aff2_image(char *driver, char *output_file, char *stream_name, 
	       char *chunks_in_segment,
	       char *append,
	       char *source) {
  ZipFile zipfile = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  char *output = talloc_strdup(zipfile, output_file);
  Image image;
  char buffer[BUFF_SIZE];
  int in_fd;
  int length;
  Link link;
  char zipfile_urn[BUFF_SIZE];

  if(!zipfile) goto error;

  in_fd=open(source,O_RDONLY);
  if(in_fd<0) {
    RaiseError(ERuntimeError, "Could not open source: %s", strerror(errno));
    goto error;
  };

  if(strstr(output_file, "file://") != output_file) {
    output = talloc_asprintf(zipfile, "file://%s", output_file);
  };

  // Make local copy of zipfile's URN
  strncpy(zipfile_urn, URNOF(zipfile), BUFF_SIZE);

  CALL((AFFObject)zipfile, set_property, "aff2:stored", output);
  if(append) {
    printf("Will append to volume %s\n", URNOF(zipfile));
    CALL((AFFObject)zipfile, set_property, "aff2volatile:append", append);
  };

  // Is it ok?
  if(!CALL((AFFObject)zipfile, finish))
    goto error;
  
  // Now we need to create an Image stream
  image = (Image)CALL(oracle, create, (AFFObject *)&__Image);
  
  // Tell the image that it should be stored in the volume
  CALL((AFFObject)image, set_property, "aff2:stored", zipfile_urn);
  if(chunks_in_segment)
    CALL((AFFObject)image, set_property, "aff2:chunks_in_segment", chunks_in_segment);

  // Is it ok?
  if(!CALL((AFFObject)image, finish))
    goto error;

  while(in_fd >= 0) {
    length = read(in_fd, buffer, BUFF_SIZE);
    if(length == 0) break;
    
    CALL((FileLikeObject)image, write, buffer, length);
  };

  CALL((FileLikeObject)image, close);

  // We want to make it easy to locate this image so we set up a link
  // to it:
  if(stream_name) {
    link = (Link)CALL(oracle, open, NULL, stream_name);
    if(!link) {
      printf("Creating a link object '%s' for stream '%s'\n", stream_name, URNOF(image));
      link = (Link)CALL(oracle, create, (AFFObject *)&__Link);
      // The link will be stored in this zipfile
      CALL((AFFObject)link, set_property, "aff2:stored", image->parent_urn);
      CALL(link, link, oracle, image->parent_urn, URNOF(image), stream_name);
      CALL((AFFObject)link, finish);
      CALL(oracle, cache_return, (AFFObject)link);
    } else {
      char *target = CALL(oracle, resolve, URNOF(link), "aff2:stored");
      printf("A link '%s' already exists to object '%s'. I will not create a link at the moment - you can make a new link later via the --link command.", URNOF(link), target);
    };
  };

  // Close the zipfile - get it back
  CALL((ZipFile)zipfile, close);
  
  // We are done with that now
  CALL(oracle, cache_return, (AFFObject)image);
  CALL(oracle, cache_return, (AFFObject)zipfile);
  
  return 0;

 error:
  if(zipfile)
    talloc_free(zipfile);
  PrintError();
  return -1;
};


int main(int argc, char **argv)
{
  int c;
  int digit_optind = 0;
  char mode=0;
  char *output_file = NULL;
  char *stream_name = "default";
  char *driver = "volume";
  char *chunks_per_segment = NULL;
  char *append = NULL;

  // Initialise the library
  AFF2_Init();

  while (1) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
      {"load", 1, 0, 'l'},
      {"append", 0, 0, 0},
      {"image", 0, 0, 'i'},
      {"driver", 1, 0, 'd'},
      {"help", 0, 0, 0},
      {"info", 0, 0, 0},
      {"output", 1, 0, 'o'},
      {"stream", 1, 0, 's'},
      {"chunks_per_segment", 1, 0, 0},
      {"verbose", 0, 0, 0},
      {"create", 1, 0, 'c'},
      {"file", 1, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, "l:iho:s:d:",
		    long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
    case 0: {
      char *option = (char *)long_options[option_index].name;
      if(!strcmp(option, "info")) {
	aff2_print_info(optarg);
      } if(!strcmp(option, "chunks_per_segment")) {
	chunks_per_segment = optarg;
      } if(!strcmp(option, "append")) {
	append = "append";
      } else {
	printf("Unknown long option %s", optarg);
	break;
      };
    };

    case 'l':
      aff2_open_volume(optarg);
      break;

    case 'o':
      output_file = optarg;
      break;

    case 'd':
      driver = optarg;
      break;

    case 's':
      stream_name = optarg;
      break;

    case 'i':
      printf("Imaging Mode selected\n");
      mode = 'i';
      break;

    case '?':
    case 'h':
      help();
      exit(0);

    default:
      printf("?? getopt returned character code 0%o ??\n", c);
    }
  }

  if (optind < argc) {
    // We are imaging now
    if(mode == 'i') {
      while (optind < argc) {
	if(!output_file) {
	  printf("You must specify an output file with --output\n");
	  exit(-1);
	};
	aff2_image(driver, output_file, stream_name, 
		   chunks_per_segment,
		   append,
		   argv[optind++]);
      };
    };
    printf("\n");
  }

  exit(EXIT_SUCCESS);
}
