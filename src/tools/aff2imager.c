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
	 "--image, -i \tImaging mode\n"
	 "--extract, -e stream_name\tExtract stream to output (set using --output or stdout)\n"
	 "--passphrase, -p password\tUse this passphrase to encrypt/decrypt the image\n"
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

void aff2_print_info(int verbose) {
  Cache i,j;

  list_for_each_entry(i, &oracle->urn->cache_list, cache_list) {
    char *urn = (char *)i->key;
    Cache attributes = (Cache)i->data;

    printf("\n******** Object %s ***********\n",urn);
    list_for_each_entry(j, &attributes->cache_list, cache_list) {
      char *key = (char *)j->key;
      char *value = (char *)j->data;

      // Ignore volatile data in default verbosity.
      if(verbose==0 && !memcmp(key, ZSTRING_NO_NULL("aff2volatile")))
	continue;

      printf("\t%s = %s\n", key, value);
    };
  };
};

/** When we want to use encryption we must create a container volume,
    A number of encrypted streams are created within the container
    volume and a new AFF2 volume is created within the encrypted
    volume. The following are the major components:

    Container aff2:stored file://filename.zip
    Image URN aff2:stored in Container
    Encrypted URN aff2:target Image
    Encrypted Volume aff2:stored in Encrypted URN
    Encrypted Image aff2:stored in Encrypted Volume

    The source is copied onto Encrypted Image
*/
int aff2_encrypted_image(char *driver, char *output_file, char *stream_name, 
			 char *chunks_in_segment,
			 char *append,
			 char *source) {
  // This is the container volume
  ZipFile zipfile, volume;
  char *output = talloc_strdup(NULL, output_file);
  Image image, encrypted;
  char buffer[BUFF_SIZE];
  int in_fd;
  int length;
  Link link;
  char zipfile_urn[BUFF_SIZE], volume_urn[BUFF_SIZE];
  char *passwd = getenv("AFF4_PASSPHRASE");

  in_fd=open(source,O_RDONLY);
  if(in_fd<0) {
    RaiseError(ERuntimeError, "Could not open source: %s", strerror(errno));
    goto error;
  };

  // Normalise this to include the full url
  if(strstr(output_file, "file://") != output_file) {
    output = talloc_asprintf(zipfile, "file://%s", output_file);
  };

  // Make local copy of zipfile's URN
  zipfile = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  if(!zipfile) goto error;

  strncpy(zipfile_urn, URNOF(zipfile), BUFF_SIZE);
  CALL((AFFObject)zipfile, set_property, "aff2:stored", output);
  if(!CALL((AFFObject)zipfile, finish))
    goto error;
  CALL(oracle, cache_return, (AFFObject)zipfile);
  
  // Now we need to create an Image stream
  image = (Image)CALL(oracle, create, (AFFObject *)&__Image);
  // Tell the image that it should be stored in the volume with no
  // compression. This is where the encrypted data is actually
  // physically stored
  CALL((AFFObject)image, set_property, "aff2:stored", zipfile_urn);
  CALL((AFFObject)image, set_property, "aff2:compression", from_int(ZIP_STORED));
  if(chunks_in_segment)
    CALL((AFFObject)image, set_property, "aff2:chunks_in_segment", chunks_in_segment);
  if(!CALL((AFFObject)image, finish))
    goto error;

  // Now we create the encrypted stream
  encrypted = (Image)CALL(oracle, create, (AFFObject *)&__Encrypted);
  
  // The encrypted object will be stored in this volume and target
  // the image
  CALL((AFFObject)encrypted, set_property, "aff2:stored", zipfile_urn);
  CALL((AFFObject)encrypted, set_property, "aff2:target", URNOF(image));
  // Dont need the original image any more
  CALL(oracle, cache_return, (AFFObject)image);

  // Initialise the crypto
  CALL((AFFObject)encrypted, set_property, "aff2volatile:passphrase", passwd);
  CALL((AFFObject)encrypted, finish);
  
  // Now we need to create an embedded volume:
  volume = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  strncpy(volume_urn, URNOF(volume), BUFF_SIZE);

  // The embedded volume lives inside the encrypted stream
  CALL((AFFObject)volume, set_property, "aff2:stored", URNOF(encrypted));
  // Is it ok?
  if(!CALL((AFFObject)volume, finish))
    goto error;
  // Done with that now
  CALL(oracle, cache_return, (AFFObject)volume);
  

  // Now we create a new image stream inside the encrypted volume:
  image = (Image)CALL(oracle, create, (AFFObject *)&__Image);
  // Tell the image that it should be stored in the volume with no
  // compression. This is where the encrypted data is actually
  // physically stored
  CALL((AFFObject)image, set_property, "aff2:stored", volume_urn);
  CALL((AFFObject)image, set_property, "aff2:compression", from_int(ZIP_DEFLATE));
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

  // Close the zipfile and dispose of it
  volume = (ZipFile)CALL(oracle, open, NULL, volume_urn);
  CALL((ZipFile)volume, close);

  // Close the zipfile and dispose of it
  zipfile = (ZipFile)CALL(oracle, open, NULL, zipfile_urn);
  CALL((ZipFile)zipfile, close);
  
  // We are done with that now
  CALL(oracle, cache_return, (AFFObject)image);
  
  return 0;

 error:
  if(zipfile)
    talloc_free(zipfile);
  PrintError();
  return -1;
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
  char *passwd = getenv("AFF4_PASSPHRASE");

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

  // Done with that now
  CALL(oracle, cache_return, (AFFObject)zipfile);
  
  // Now we need to create an Image stream
  image = (Image)CALL(oracle, create, (AFFObject *)&__Image);
  
  // Tell the image that it should be stored in the volume
  CALL((AFFObject)image, set_property, "aff2:stored", zipfile_urn);
  if(chunks_in_segment)
    CALL((AFFObject)image, set_property, "aff2:chunks_in_segment", chunks_in_segment);

  // Is it ok?
  if(!CALL((AFFObject)image, finish))
    goto error;

  // Do we need to encrypt the image?
  if(passwd) {
    Image encrypted = (Image)CALL(oracle, create, (AFFObject *)&__Encrypted);

    // The encrypted object will be stored in this volume and target
    // the image
    CALL((AFFObject)encrypted, set_property, "aff2:stored", zipfile_urn);
    CALL((AFFObject)encrypted, set_property, "aff2:target", URNOF(image));
    CALL((AFFObject)encrypted, set_property, "aff2volatile:passphrase", passwd);
    CALL((AFFObject)encrypted, finish);

    // Dont need the original image any more
    CALL(oracle, cache_return, (AFFObject)image);

    // Make sure that we write on the encrypted stream now
    image = encrypted;
  };

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
      CALL((AFFObject)link, set_property, "aff2:stored", zipfile_urn);
      CALL(link, link, oracle, zipfile_urn, URNOF(image), stream_name);
      CALL((AFFObject)link, finish);
      CALL(oracle, cache_return, (AFFObject)link);
    } else {
      char *target = CALL(oracle, resolve, URNOF(link), "aff2:stored");
      printf("A link '%s' already exists to object '%s'. I will not create a link at the moment - you can make a new link later via the --link command.", URNOF(link), target);
    };
  };

  // Close the zipfile and dispose of it
  zipfile = (ZipFile)CALL(oracle, open, NULL, zipfile_urn);
  CALL((ZipFile)zipfile, close);
  talloc_free(zipfile);
  
  // We are done with that now
  CALL(oracle, cache_return, (AFFObject)image);
  
  return 0;

 error:
  if(zipfile)
    talloc_free(zipfile);
  PrintError();
  return -1;
};

void aff2_extract(char *stream, char *output_file) {
  FileLikeObject in_fd = (FileLikeObject)CALL(oracle, open, NULL, stream);
  FileLikeObject out_fd;

  if(!in_fd) {
    printf("Stream %s not found!!!\n", stream);
    return;
  };

  // Create a new FileLikeObject for the output
  if(strstr(output_file, "file://") != output_file) {
    char *tmp = talloc_asprintf(NULL, "file://%s", output_file);
    out_fd = (FileLikeObject)CALL(oracle, open, NULL, tmp);
    talloc_free(tmp);
  } else {
    out_fd = (FileLikeObject)CALL(oracle, open, NULL, output_file);
  }; 

  if(!out_fd) 
    return;

  // Now copy the input stream to the output stream
  while(1) {
    char buff[BUFF_SIZE];
    int len = CALL(in_fd, read, buff, BUFF_SIZE);

    if(len<=0) break;
    CALL(out_fd, write, buff, len);
  };

  CALL(oracle, cache_return, (AFFObject)in_fd);
  CALL(oracle, cache_return, (AFFObject)out_fd);
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
  int verbose=0;
  char *extract = NULL;
  char *passphrase = NULL;

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
      {"info", 0, 0, 'I'},
      {"output", 1, 0, 'o'},
      {"stream", 1, 0, 's'},
      {"chunks_per_segment", 1, 0, 0},
      {"verbose", 0, 0, 'v'},
      {"create", 1, 0, 'c'},
      {"extract", 1, 0, 'e'},
      {"passphrase", 1, 0, 'p'},
      {"file", 1, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, "l:iIho:s:d:ve:p:",
		    long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
    case 0: {
      char *option = (char *)long_options[option_index].name;
      if(!strcmp(option, "chunks_per_segment")) {
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

    case 'e':
      extract = optarg;

    case 's':
      stream_name = optarg;
      break;

    case 'p':
      setenv("AFF4_PASSPHRASE", optarg, 1);
      break;

    case 'i':
      printf("Imaging Mode selected\n");
      mode = 'i';
      break;

    case 'I':
      printf("Info mode selected\n");
      mode = 'I';
      break;

    case 'v':
      verbose++;
      break;

    case '?':
    case 'h':
      help();
      exit(0);

    default:
      printf("?? getopt returned character code 0%o ??\n", c);
    }
  }

  // Do we want to extract a stream:
  if(extract) {
    aff2_extract(extract, output_file);
  } 
  // Do we just want to print the content of the resolver?
  else if(mode == 'I') {
    aff2_print_info(verbose);
  } else if (optind < argc) {
    // We are imaging now
    if(mode == 'i') {
      while (optind < argc) {
	if(!output_file) {
	  printf("You must specify an output file with --output\n");
	  exit(-1);
	};
	if(getenv("AFF4_PASSPHRASE")) {
	  aff2_encrypted_image(driver, output_file, stream_name,
			       chunks_per_segment,
			       append,
			       argv[optind++]);
	} else {
	  aff2_image(driver, output_file, stream_name, 
		     chunks_per_segment,
		     append,
		     argv[optind++]);
	};
      };
    };
    printf("\n");
  }

  exit(EXIT_SUCCESS);
}
