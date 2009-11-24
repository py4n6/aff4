#include "zip.h"
#include "aff4.h"
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include "common.h"
#include <libgen.h>

#define IMAGE_BUFF_SIZE (1024*1024)

void aff2_open_volume(char *urn, char mode) {
  // Try the different volume implementations in turn until one works:
  ZipFile volume;

  volume = open_volume(urn, mode);

  CALL(oracle, cache_return, (AFFObject)volume);
};

void aff2_print_info(int verbose) {
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

#if 0
  // This is the container volume
  ZipFile container_volume, volume;
  char *output = talloc_strdup(NULL, output_file);
  FileLikeObject storage_image, image, encrypted;
  char buffer[BUFF_SIZE];
  int in_fd;
  int length;
  char container_volume_urn[BUFF_SIZE], volume_urn[BUFF_SIZE], encrypted_urn[BUFF_SIZE];
  char *passwd = getenv("AFF4_PASSPHRASE");

  in_fd=open(source,O_RDONLY);
  if(in_fd<0) {
    RaiseError(ERuntimeError, "Could not open source: %s", strerror(errno));
    goto error;
  };

  container_volume = create_volume(driver);
  if(!container_volume) goto error;

  CALL((AFFObject)container_volume, set_property, AFF4_STORED, normalise_url(output));
  if(!CALL((AFFObject)container_volume, finish))
    goto error;

  // Make local copy of container_volume URN
  strncpy(container_volume_urn, URNOF(container_volume), BUFF_SIZE);
  CALL(oracle, cache_return, (AFFObject)container_volume);
  
  // Now we need to create an Image stream to store the encrypted volume
  storage_image = (FileLikeObject)CALL(oracle, create, (AFFObject *)&__Image);
  // Tell the image that it should be stored in the volume with no
  // compression. This is where the encrypted data is actually
  // physically stored
  CALL((AFFObject)storage_image, set_property, AFF4_STORED, container_volume_urn);
  CALL(oracle, add, container_volume_urn, AFF4_CONTAINS, URNOF(storage_image));
  CALL((AFFObject)storage_image, set_property, AFF4_COMPRESSION, from_int(ZIP_STORED));
  if(chunks_in_segment)
    CALL((AFFObject)storage_image, set_property, AFF4_CHUNKS_IN_SEGMENT, chunks_in_segment);
  if(!CALL((AFFObject)storage_image, finish))
    goto error;

  // The encrypted object will be stored in this volume and target (be
  // backed by) the image:
  encrypted = (FileLikeObject)CALL(oracle, create, (AFFObject *)&__Encrypted);
  CALL((AFFObject)encrypted, set_property, AFF4_STORED, container_volume_urn);
  CALL(oracle, add, container_volume_urn, AFF4_CONTAINS, URNOF(encrypted));
  CALL((AFFObject)encrypted, set_property, AFF4_TARGET, URNOF(storage_image));
  // Dont need the original image any more
  CALL(oracle, cache_return, (AFFObject)storage_image);

  // Initialise the crypto
  CALL((AFFObject)encrypted, set_property, AFF4_VOLATILE_PASSPHRASE, passwd);
  if(!CALL((AFFObject)encrypted, finish))
    goto error;

  strncpy(encrypted_urn, URNOF(encrypted), BUFF_SIZE);
  CALL(oracle, cache_return, (AFFObject)encrypted);

  // Set an autoload to make the loader open the encrypted volume when
  // the container volume is opened:
  CALL(oracle, add, container_volume_urn, AFF4_AUTOLOAD, encrypted_urn);

  // Now we need to create an embedded volume (embedded volumes are
  // always Zip containers). The embedded volume is stored in the
  // encrypted stream:
  volume = (ZipFile)CALL(oracle, create, (AFFObject *)&__ZipFile);
  strncpy(volume_urn, URNOF(volume), BUFF_SIZE);

  // The embedded volume lives inside the encrypted stream
  CALL((AFFObject)volume, set_property, AFF4_STORED, encrypted_urn);
  // Is it ok?
  if(!CALL((AFFObject)volume, finish))
    goto error;
  // Done with that now
  CALL(oracle, cache_return, (AFFObject)volume);

  // Now we create a new image stream inside the encrypted
  // volume. Thats where we stored the disk image:
  image = (FileLikeObject)CALL(oracle, create, (AFFObject *)&__Image);
  CALL((AFFObject)image, set_property, AFF4_STORED, volume_urn);
  CALL(oracle, add, volume_urn, AFF4_CONTAINS, URNOF(image));
  CALL((AFFObject)image, set_property, AFF4_COMPRESSION, from_int(ZIP_DEFLATE));
  //CALL((AFFObject)image, set_property, AFF4_COMPRESSION, from_int(ZIP_STORED));
  if(chunks_in_segment)
    CALL((AFFObject)image, set_property, AFF4_CHUNKS_IN_SEGMENT, chunks_in_segment);
  // Is it ok?
  if(!CALL((AFFObject)image, finish))
    goto error;

  // Now copy the data into the image
  while(in_fd >= 0) {
    length = read(in_fd, buffer, BUFF_SIZE);
    if(length == 0) break;
    
    CALL((FileLikeObject)image, write, buffer, length);
  };

  CALL((FileLikeObject)image, close);

  // Close the zipfile and dispose of it
  volume = (ZipFile)CALL(oracle, open, volume_urn, 'w');
  CALL((ZipFile)volume, close);

  // Close the zipfile and dispose of it
  container_volume = (ZipFile)CALL(oracle, open, container_volume_urn, 'w');
  CALL((ZipFile)container_volume, close);
  
  return 0;

 error:
  if(container_volume)
    talloc_free(container_volume);
  PrintError();
  return -1;
#endif
};

/** This one creates a regular image on the output_file */
int aff4_image(char *driver, char *output_file, char *stream_name, 
	       unsigned int chunks_in_segment,
	       uint64_t max_size,
	       char *in_urn) {
  ZipFile zipfile;
  char *output;
  Image image;
  char buffer[IMAGE_BUFF_SIZE];
  int length;
  int count = 0;
  RDFURN output_urn = (RDFURN)rdfvalue_from_urn(NULL, output_file);
  XSDInteger directory_offset = new_XSDInteger(output_urn);
  RDFURN input_urn = (RDFURN)rdfvalue_from_urn(output_urn, in_urn);
  FileLikeObject in_fd = CALL(oracle, open, input_urn, 'r');

  if(!in_fd) {
    goto error;
  };

  zipfile = (ZipFile)CALL(oracle, create, driver);
  if(!zipfile) {
    PrintError();
    print_volume_drivers();
    goto error;
  };

  CALL((AFFObject)zipfile, set_property, AFF4_STORED, 
       (RDFValue)output_urn);

  // Is it ok?
  if(!CALL((AFFObject)zipfile, finish)) {
    goto error;
  };


  // Now we need to create an Image stream
  image = (Image)CALL(oracle, create, AFF4_IMAGE);

  // Tell the image that it should be stored in the volume
  CALL((AFFObject)image, set_property, AFF4_STORED, (RDFValue)URNOF(zipfile));
  if(chunks_in_segment > 0)
    CALL((AFFObject)image, set_property, AFF4_CHUNKS_IN_SEGMENT, 
         rdfvalue_from_int(output_urn, chunks_in_segment));

  // Done with the volume now
  CALL(oracle, cache_return, (AFFObject)zipfile);

  // Is it ok?
  if(!CALL((AFFObject)image, finish))
    goto error;

  while(1) {
    // Check if we need to change volumes:
    if(max_size > 0) {
      CALL(oracle, resolve_value, URNOF(zipfile), AFF4_DIRECTORY_OFFSET, 
           (RDFValue)directory_offset);

      if(directory_offset->value > max_size) {
	ZipFile zip = (ZipFile)CALL(oracle, open, URNOF(zipfile), 'w');

	char buff[BUFF_SIZE];
	snprintf(buff, BUFF_SIZE, "%s.%03u", output_file, count++);

        CALL(output_urn, set, buff);

	// Leave a hint in this volume to load the next volume
	CALL(oracle, add_value, URNOF(zipfile), AFF4_AUTOLOAD,
             (RDFValue)output_urn);

	CALL(zip, close);

	// Create a new volume to hold the image:
	zip = (ZipFile)CALL(oracle, create, driver);
	CALL((AFFObject)zip, set_property, AFF4_STORED, 
             (RDFValue)output_urn);

	// Is it ok?
	if(!CALL((AFFObject)zip, finish)) {
	  goto error;
	};

	// Tell the resolver that from now on the image will be stored
	// on the new volume:
	CALL(oracle, set_value, URNOF(image), AFF4_STORED, 
             (RDFValue)URNOF(zip));
	CALL(oracle, cache_return, (AFFObject)zip);
      };
    };

    length = CALL(in_fd, read, buffer, IMAGE_BUFF_SIZE);
    if(length == 0) break;
    
    CALL((FileLikeObject)image, write, buffer, length);
  };

  CALL((FileLikeObject)image, close);

  // Close the zipfile and dispose of it
  zipfile = (ZipFile)CALL(oracle, open, URNOF(zipfile), 'w');
  CALL((ZipFile)zipfile, close);

  return 0;

 error:
  PrintError();
  return -1;
};

#if 0
void aff2_extract(char *stream, char *output_file) {
  FileLikeObject in_fd = (FileLikeObject)CALL(oracle, open, stream, 'r');
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
  CALL(in_fd, seek, 0, SEEK_SET);
  while(1) {
    char buff[BUFF_SIZE * 10];
    int len = CALL(in_fd, read, buff, BUFF_SIZE * 10);

    if(len==0) break;
    if(len<0) {
      RaiseError(ERuntimeError, "Error");
      break;
    };
    CALL(out_fd, write, buff, len);
  };

  CALL(oracle, cache_return, (AFFObject)in_fd);
  CALL(out_fd, close);
};

static uint64_t current_pos=0;
static char *current_uri = NULL;

static int progress_cb(uint64_t progress, char *urn) {
  int tmp = progress >> 20;
  char *ctx = talloc_size(NULL, 1);

  if(current_uri==NULL) current_uri=urn;

  if(current_uri != urn) {
    char *type = CALL(oracle, resolve, ctx, urn, AFF4_TYPE);
    char *real_hash = CALL(oracle, resolve, ctx, current_uri, AFF4_SHA);
    Resolver i;
    char *result="";

    list_for_each_entry(i, &oracle->identities, identities) {
      char *signed_uri = CALL(i, resolve, ctx, current_uri, AFF4_SHA);

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
    char *type = CALL(oracle, resolve, ctx, urn, AFF4_TYPE);
    if(type) {
      printf("\r%s (%s): %d MB", urn, type, tmp);
    } else {
      printf("\r%s:\n: %d MB", urn, tmp);
    };
    current_pos = tmp;
  };

  fflush(stdout);

  talloc_free(ctx);
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
      char *ctx = talloc_size(NULL, 1);
      // Make sure its actually an Identity object:
      if(!strcmp(AFF4_IDENTITY, CALL(oracle, resolve, ctx, key, AFF4_TYPE))) {
	Identity id = (Identity)CALL(oracle, open, key, 'r');
	
	CALL(id, verify, progress_cb);
	CALL(oracle, cache_return, (AFFObject)id);
      };
      talloc_free(ctx);
    };
  };
};
#endif

int main(int argc, char **argv)
{
  int c;
  char mode=0;
  char *output_file = NULL;
  char *stream_name = "default";
  char *driver = AFF4_ZIP_VOLUME;
  int chunks_per_segment = 0;
  char *append = NULL;
  int verbose=0;
  char *extract = NULL;
  char *cert = NULL;
  char *key_file = NULL;
  int verify = 0;
  uint64_t max_size=0;

  // Initialise the library
  AFF4_Init();

  //talloc_enable_leak_report_full();

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
      {"max_size\0"
       "When a volume exceeds this size, a new volume is created (default 0-unlimited)",
       1, 0, 0},
      {"stream\0"
       "If specified a link will be added with this name to the new stream", 1, 0, 's'},
      {"cert\0"
       "Certificate to use for signing", 1, 0, 'c'},
      {"key\0"
       "Private key in PEM format (needed for signing)", 1, 0, 'k'},
      {"passphrase\0"
       "Encrypt the volume using this passphrase", 1, 0, 'p'},

      {"info\0"
       "*Information mode (print information on all objects in this volume)", 0, 0, 'I'},
      {"load\0"
       "Open this file and populate the resolver (can be provided multiple times)", 1, 0, 'l'},
      {"no_autoload\0"
       "Do not automatically load volumes (affects subsequent --load)", 0, 0, 0},

      {"extract\0"
       "*Extract mode (dump the content of stream)", 1, 0, 'e'},

      {"verify\0"
       "*Verify all Identity objects in these volumes and report on their validity",0, 0, 'V'},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, generate_short_optargs(long_options),
		    long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {
    case 0: {
      char *option = (char *)long_options[option_index].name;
      if(!strcmp(option, "chunks_per_segment")) {
	chunks_per_segment = parse_int(optarg);
	break;
      } if(!strcmp(option, "max_size")) {
	max_size = parse_int(optarg);
	break;
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
      //CALL(oracle, set_value, GLOBAL, AFF4_VOLATILE_PASSPHRASE, optarg);
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

#if 0
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
  } else
#endif
    if(optind < argc) {
      // We are imaging now
      if(mode == 'i') {
        while (optind < argc) {
          if(!output_file) {
            printf("You must specify an output file with --output\n");
            exit(-1);
          };

          aff4_image(driver, output_file, stream_name,
                     chunks_per_segment,
                     max_size,
                     argv[optind++]);
        };
      };
      printf("\n");
    };

  PrintError();
  exit(EXIT_SUCCESS);
}
