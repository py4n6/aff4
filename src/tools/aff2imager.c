#include <getopt.h>
#include "zip.h"
#include "aff4.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

void print_help(struct option *opts) {
  struct option *i;

  printf("The following options are supported\n");

  for(i=opts; i->name; i++) {
    char *description = (char *)i->name + strlen(i->name) +1;
    char short_opt[BUFF_SIZE];
    char *prefix="\t";

    if(description[0]=='*') {
      prefix="\n";
      description++;
    };

    if(i->val) 
      snprintf(short_opt, BUFF_SIZE, ", -%c", i->val);
    else
      short_opt[0]=0;

    if(i->has_arg) {
      printf("%s--%s%s [ARG]\t%s\n", prefix, i->name, short_opt, description);
    } else {
      printf("%s--%s%s\t\t%s\n", prefix,i->name, short_opt, description);
    };
  };
};

void list_info() {
  char *result =  CALL(oracle, export_all, "");
  printf("%s", result);

  talloc_free(result);
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

ZipFile create_volume(char *driver) {
  if(!strcmp(driver, "volume")) {
    return (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  } else if(!strcmp(driver, "directory")) {
    return (ZipFile)CALL(oracle, create, (AFFObject *)&__DirVolume);
  } else {
    RaiseError(ERuntimeError, "Unknown driver %s", driver);
    return NULL;
  };
};

void aff2_open_volume(char *urn, char mode) {
  // Try the different volume implementations in turn until one works:
  ZipFile volume;

  volume = open_volume(urn, mode);

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
      Resolver i;

      // Ignore volatile data in default verbosity.
      if(verbose==0 && !memcmp(key, ZSTRING_NO_NULL(VOLATILE_NS)))
	continue;

      printf("\t%s = %s\n", key, value);
      /** Now print which identity vouches for this fact */
      list_for_each_entry(i, &oracle->identities, identities) {
	char *hash = CALL(i, resolve, urn, key);

	if(hash) {
	  if(!strcmp(hash, value)) {
	    printf("\t\t---> Signed by %s\n", URNOF(i));
	  } else {
	    printf("\t\t---> Incorrect %s says it should be %s\n", URNOF(i), hash);
	  };
	};
      };
    };
  };
};

/** When we want to use encryption we must create a container volume,
    A number of encrypted streams are created within the container
    volume and a new AFF2 volume is created within the encrypted
    volume. The following are the major components:

    Container AFF4_STORED file://filename.zip
    Image URN AFF4_STORED in Container
    Encrypted URN AFF4_TARGET Image
    Encrypted Volume AFF4_STORED in Encrypted URN
    Encrypted Image AFF4_STORED in Encrypted Volume

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
  char zipfile_urn[BUFF_SIZE], volume_urn[BUFF_SIZE];
  char *passwd = getenv("AFF4_PASSPHRASE");

  in_fd=open(source,O_RDONLY);
  if(in_fd<0) {
    RaiseError(ERuntimeError, "Could not open source: %s", strerror(errno));
    goto error;
  };

  zipfile = create_volume(driver);
  if(!zipfile) goto error;

  if(!strstr(output_file, ":")) {
    output = talloc_asprintf(zipfile, "file://%s", output_file);
  };

  CALL((AFFObject)zipfile, set_property, AFF4_STORED, output);
  if(!CALL((AFFObject)zipfile, finish))
    goto error;

  // Make local copy of zipfile's URN
  strncpy(zipfile_urn, URNOF(zipfile), BUFF_SIZE);
  CALL(oracle, cache_return, (AFFObject)zipfile);
  
  // Now we need to create an Image stream
  image = (Image)CALL(oracle, create, (AFFObject *)&__Image);
  // Tell the image that it should be stored in the volume with no
  // compression. This is where the encrypted data is actually
  // physically stored
  CALL((AFFObject)image, set_property, AFF4_STORED, zipfile_urn);
  CALL((AFFObject)image, set_property, AFF4_COMPRESSION, from_int(ZIP_STORED));
  if(chunks_in_segment)
    CALL((AFFObject)image, set_property, AFF4_CHUNKS_IN_SEGMENT, chunks_in_segment);
  if(!CALL((AFFObject)image, finish))
    goto error;

  // Now we create the encrypted stream
  encrypted = (Image)CALL(oracle, create, (AFFObject *)&__Encrypted);
  
  // The encrypted object will be stored in this volume and target
  // the image
  CALL((AFFObject)encrypted, set_property, AFF4_STORED, zipfile_urn);
  CALL((AFFObject)encrypted, set_property, AFF4_TARGET, URNOF(image));
  // Dont need the original image any more
  CALL(oracle, cache_return, (AFFObject)image);

  // Initialise the crypto
  CALL((AFFObject)encrypted, set_property, "aff2volatile:passphrase", passwd);
  CALL((AFFObject)encrypted, finish);
  
  // Now we need to create an embedded volume:
  volume = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  strncpy(volume_urn, URNOF(volume), BUFF_SIZE);

  // The embedded volume lives inside the encrypted stream
  CALL((AFFObject)volume, set_property, AFF4_STORED, URNOF(encrypted));
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
  CALL((AFFObject)image, set_property, AFF4_STORED, volume_urn);
  CALL((AFFObject)image, set_property, AFF4_COMPRESSION, from_int(ZIP_DEFLATE));
  if(chunks_in_segment)
    CALL((AFFObject)image, set_property, AFF4_CHUNKS_IN_SEGMENT, chunks_in_segment);
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
  volume = (ZipFile)CALL(oracle, open, NULL, volume_urn, 'w');
  CALL((ZipFile)volume, close);

  // Close the zipfile and dispose of it
  zipfile = (ZipFile)CALL(oracle, open, NULL, zipfile_urn, 'w');
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
  ZipFile zipfile = create_volume(driver);
  char *output;
  Image image;
  char buffer[BUFF_SIZE];
  int in_fd;
  int length;
  Link link;
  char zipfile_urn[BUFF_SIZE];
  char *passwd = getenv("AFF4_PASSPHRASE");

  if(!zipfile) goto error;

  output = talloc_strdup(zipfile, output_file);
  in_fd=open(source,O_RDONLY);
  if(in_fd<0) {
    RaiseError(ERuntimeError, "Could not open source: %s", strerror(errno));
    goto error;
  };

  // We assume that if the name does not contain a : its a filename
  // and append a file:// reference to it. Of course we expect a fully
  // qualified URI (e.g. http://foobar/), but this is a user friendly
  // feature.
  if(!strstr(output_file, ":")) {
    output = talloc_asprintf(zipfile, "file://%s", output_file);
  };

  CALL((AFFObject)zipfile, set_property, AFF4_STORED, output);

  // Is it ok?
  if(!CALL((AFFObject)zipfile, finish)) {
    goto error;
  };

  // Make local copy of zipfile's URN
  strncpy(zipfile_urn, URNOF(zipfile), BUFF_SIZE);

  // Done with that now
  CALL(oracle, cache_return, (AFFObject)zipfile);
  
  // Now we need to create an Image stream
  image = (Image)CALL(oracle, create, (AFFObject *)&__Image);
  
  // Tell the image that it should be stored in the volume
  CALL((AFFObject)image, set_property, AFF4_STORED, zipfile_urn);
  if(chunks_in_segment)
    CALL((AFFObject)image, set_property, AFF4_CHUNKS_IN_SEGMENT, chunks_in_segment);

  // Is it ok?
  if(!CALL((AFFObject)image, finish))
    goto error;

  // Do we need to encrypt the image?
  if(passwd) {
    Image encrypted = (Image)CALL(oracle, create, (AFFObject *)&__Encrypted);

    // The encrypted object will be stored in this volume and target
    // the image
    CALL((AFFObject)encrypted, set_property, AFF4_STORED, zipfile_urn);
    CALL((AFFObject)encrypted, set_property, AFF4_TARGET, URNOF(image));
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
    PrintError();
    link = (Link)CALL(oracle, open, NULL,
		      fully_qualified_name(stream_name, URNOF(image)),
		      'r');
    ClearError();
    if(!link) {
      printf("Creating a link object '%s' for stream '%s'\n", stream_name, URNOF(image));
      link = (Link)CALL(oracle, create, (AFFObject *)&__Link);
      // The link will be stored in this zipfile
      CALL((AFFObject)link, set_property, AFF4_STORED, zipfile_urn);
      CALL(link, link, oracle, zipfile_urn, URNOF(image), stream_name);
      CALL((AFFObject)link, finish);
      CALL(oracle, cache_return, (AFFObject)link);
    } else {
      char *target = CALL(oracle, resolve, URNOF(link), AFF4_STORED);
      printf("A link '%s' already exists to object '%s'. I will not create a link at the moment - you can make a new link later via the --link command.", URNOF(link), target);
    };
  };

  // Sign if needed
  {
    Resolver i;

    list_for_each_entry(i, &oracle->identities, identities) {
      CALL(i->identity, store, zipfile_urn);
    };
  };

  // Close the zipfile and dispose of it
  zipfile = (ZipFile)CALL(oracle, open, NULL, zipfile_urn, 'w');
  CALL((ZipFile)zipfile, close);
  talloc_free(zipfile);
  
  // We are done with that now
  CALL(oracle, cache_return, (AFFObject)image);
  
  return 0;

 error:
  PrintError();
  return -1;
};

void aff2_extract(char *stream, char *output_file) {
  FileLikeObject in_fd = (FileLikeObject)CALL(oracle, open, NULL, stream, 'r');
  FileLikeObject out_fd;
  
  if(!in_fd) {
    printf("Stream %s not found!!!\n", stream);
    return;
  };

  // Create a new FileLikeObject for the output
  out_fd = (FileLikeObject)CALL(oracle, create, (AFFObject *)&__FileBackedObject);
  if(strstr(output_file, "file://") != output_file) {
    URNOF(out_fd) = talloc_asprintf(out_fd, "file://%s", output_file);
  } else {
    URNOF(out_fd) = talloc_strdup(out_fd, output_file);
  }; 

  if(!CALL((AFFObject)out_fd, finish)) 
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

static uint64_t current_pos=0;
static char *current_uri = NULL;

static int progress_cb(uint64_t progress, char *urn) {
  int tmp = progress >> 20;

  if(current_uri==NULL) current_uri=urn;

  if(current_uri != urn) {
    char *type = CALL(oracle, resolve, urn, AFF4_TYPE);
    char *real_hash = CALL(oracle, resolve, current_uri, AFF4_SHA);
    Resolver i;
    char *result="";

    list_for_each_entry(i, &oracle->identities, identities) {
      char *signed_uri = CALL(i, resolve, current_uri, AFF4_SHA);

      if(signed_uri && real_hash) {
	if(!strcmp(signed_uri, real_hash)) {
	  result = "OK";
	  break;
	} else {
	  result = "Hash Mismatch";
	};
      } else {
	result = "";
      };
    };

    if(type)
      printf("\r%s (%s): (%s)              \n", current_uri, type, result); 
    else 
      printf("\r%s: (%s)              \n", current_uri, result); 
    
    current_uri = urn;
  };

  if(current_pos != tmp) {
    char *type = CALL(oracle, resolve, urn, AFF4_TYPE);
    if(type) {
      printf("\r%s (%s): %d MB", urn, type, tmp);
    } else {
      printf("\r%s:\n: %d MB", urn, tmp);
    };
    current_pos = tmp;
  };

  fflush(stdout);

  return 1;
};

/** Verify all the signatures in the volumes. This essentially just
    loads all identities and asks them to verify their statements.
*/
void aff2_verify() {
  Cache i;

  list_for_each_entry(i, &oracle->urn->cache_list, cache_list) {
    char *key = (char *)i->key;

    if(startswith(key, AFF4_IDENTITY_PREFIX)) {
      // Make sure its actually an Identity object:
      if(!strcmp(AFF4_IDENTITY, CALL(oracle, resolve, key, AFF4_TYPE))) {
	Identity id = (Identity)CALL(oracle, open, NULL, key, 'r');
	
	CALL(id, verify, progress_cb);
	CALL(oracle, cache_return, (AFFObject)id);
      };
    };
  };
};


int main(int argc, char **argv)
{
  int c;
  char mode=0;
  char *output_file = NULL;
  char *stream_name = "default";
  char *driver = "volume";
  char *chunks_per_segment = NULL;
  char *append = NULL;
  int verbose=0;
  char *extract = NULL;
  char *passphrase = NULL;
  char *cert = NULL;
  char *key_file = NULL;
  int verify = 0;

  // Initialise the library
  AFF2_Init();

  talloc_enable_leak_report_full();

  while (1) {
    int option_index = 0;
    // Note that we use an extension to long_options to allow the
    // helpful descriptions to be included with the long names. This
    // keeps everything well synchronised in the same place.
    static struct option long_options[] = {
      {"help\0"
       "*This message", 0, 0, 'h'},
      {"verbose\0"
       "*Verbose (can be specified more than once)", 0, 0, 'v'},

      {"image\0"
       "*Imaging mode (Image each argv as a new stream)", 0, 0, 'i'},
      {"driver\0"
       "Which driver to use - 'directory' or 'volume' (Zip archive, default)", 1, 0, 'd'},
      {"output\0"
       "Create the output volume on this file or URL (using webdav)", 1, 0, 'o'},
      {"chunks_per_segment\0"
       "How many chunks in each segment of the image (default 2048)", 1, 0, 0},
      {"stream\0"
       "If specified a link will be added with this name to the new stream", 1, 0, 's'},
      {"cert\0"
       "Certificate to use for signing", 1, 0, 'c'},
      {"key\0"
       "Private key in PEM format", 1, 0, 'k'},

      {"info\0"
       "*Information mode (print information on all objects in this volume)", 0, 0, 'I'},
      {"load\0"
       "Open this file and populate the resolver (can be provided multiple times)", 1, 0, 'l'},

      {"extract\0"
       "*Extract mode (dump the content of stream)", 1, 0, 'e'},

      {"verify\0"
       "*Verify all Identity objects in these volumes and report on their validity",0, 0, 'V'},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, "l:iIho:s:d:ve:p:c:k:V",
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
      aff2_open_volume(optarg, 'r');
      break;

    case 'c':
      cert = optarg;
      break;

    case 'k':
      key_file = optarg;
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

    case 'V':
      verify = 1;
      break;

    case '?':
    case 'h':
      printf("%s - an AFF4 general purpose imager.\n", argv[0]);
      print_help(long_options);
      exit(0);

    default:
      printf("?? getopt returned character code 0%o ??\n", c);
    }
  }

  if(cert || key_file)
    add_identity(key_file, cert);

  // Do we want to extract a stream:
  if(extract) {
    if(!output_file) output_file = "/dev/stdout";
    aff2_extract(extract, output_file);
  } 
  // Do we just want to print the content of the resolver?
  else if(mode == 'I') {
    if(verify)
      aff2_verify();

    aff2_print_info(verbose);
  } else if(optind < argc) {
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

  PrintError();
  exit(EXIT_SUCCESS);
}
