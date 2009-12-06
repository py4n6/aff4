/*
** raid_reconstructor.c
**

This program gives a visual aid for reassembling a RAID5 set. It can
be used to then build an AFF4 map of the raid set in a new volume.

** Made by (mic)
** Login   <scudette@gmail.com>
**
** Started on  Tue Nov 24 21:04:39 2009 mic
** Last update Sun May 12 01:17:25 2002 Speed Blue
*/

#include "aff4.h"
#include <getopt.h>
#include "common.h"

static char *seperator = " | ";

struct _map_point {
  int block;
  FileLikeObject target;
};

struct map_description {
  int **description;
  int image_period;
  int target_period;
  int blocksize;
  uint64_t size;
  struct _map_point *map;
} map_description;

struct map_description *parse_map(char *map, int number_of_disks, int *period,
                                  int blocksize,
                                  FileLikeObject *disks) {
  struct map_description *result;
  int len = strlen(map);
  int number_of_elements=1,i,j,k;
  int last=0;

  for(i=0; i<len; i++) {
    if(map[i]==',') number_of_elements++;
  };

  if(number_of_elements % number_of_disks) {
    RaiseError(ERuntimeError, "Map does not have the correct number of elements (%u) must be a multiple of %u", number_of_elements, number_of_disks);
    return NULL;
  };

  *period = number_of_elements / number_of_disks;

  // Make the data structure now
  result = talloc(NULL, struct map_description);
  result->description = talloc_array(result, int *, *period);
  result->map = talloc_array(result, struct _map_point,
                             *period * (number_of_disks - 1));
  result->image_period = 0;
  result->target_period = *period;
  result->blocksize = blocksize;
  result->size = disks[0]->size->value * (number_of_disks - 1);

  for(i=0; i<*period; i++) {
    result->description[i] = talloc_array(result->description, int, number_of_disks);
  };

  // Now fill it in:
  j=0;
  k=0;
  for(i=0; i<len+1; i++) {
    if(map[i]==',' || i==len) {
      map[i]=0;

      if(map[last]=='p' || map[last]=='P') result->description[j][k]=-1;
      else {
        int image_block = strtol(map + last, NULL, 0);
        result->description[k][j] = image_block;

        if(image_block < 0 || image_block > *period * (number_of_disks - 1)) {
          RaiseError(ERuntimeError, "Image point %u is outside acceptable range", image_block);
          goto error;
        };

        result->map[image_block].block = k;
        result->map[image_block].target = disks[j];
        result->image_period = max(result->image_period, image_block + 1);
      };
      j++;
      if(j>=number_of_disks) {
        j=0;
        k++;
      };
      last = i+1;
    };
  };

  return result;
 error:
  if(result)
    talloc_free(result);
  return NULL;
};

void make_map_stream(char *driver, struct map_description *map, char *output, char *stream) {
  RDFURN output_urn = (RDFURN)rdfvalue_from_urn(NULL, output);
  ZipFile zip = (ZipFile)CALL(oracle, create, driver, 'w');
  MapDriver map_fd = (MapDriver)CALL(oracle, create, AFF4_MAP, 'w');
  XSDInteger i = new_XSDInteger(output_urn);

  CALL(oracle, set_value, URNOF(zip), AFF4_STORED, (RDFValue)output_urn);
  if(!CALL((AFFObject)zip, finish))
    goto exit;

  URNOF(map_fd) = CALL(URNOF(zip), copy, map_fd);
  CALL(URNOF(map_fd), add, stream);

  CALL(oracle, set_value, URNOF(map_fd), AFF4_STORED, 
       (RDFValue)URNOF(zip));

  CALL(oracle, cache_return, (AFFObject)zip);

  if(!CALL((AFFObject)map_fd, finish))
    goto exit;

  CALL(i, set, map->image_period);
  CALL(oracle, set_value, URNOF(map_fd), AFF4_IMAGE_PERIOD, (RDFValue)i);

  CALL(i, set, map->target_period);
  CALL(oracle, set_value, URNOF(map_fd), AFF4_TARGET_PERIOD, (RDFValue)i);

  CALL(i, set, map->blocksize);
  CALL(oracle, set_value, URNOF(map_fd), AFF4_BLOCKSIZE, (RDFValue)i);

  CALL(map_fd->super.size, set, map->size);

  for(i->value = 0; i->value < map->image_period; i->value++) {
    CALL(map_fd, add, i->value, map->map[i->value].block,
         URNOF(map->map[i->value].target)->value);
  };

  CALL((FileLikeObject)map_fd, close);
  CALL(zip, close);

 exit:
  if(output_urn)
    talloc_free(output_urn);

  return;
};


int print_row(FileLikeObject *disks, int number_of_disks, int columnwidth) {
  int i;
  int min_count = columnwidth;

  for(i=0;i<number_of_disks;i++) {
    char buff[columnwidth+1];
    int j;
    int count = 0;

    if(CALL(disks[i], read, buff, columnwidth) < columnwidth)
      return 0;

    for(j=0;j<columnwidth;j++) {
      if(buff[j] < 32 || buff[j] >= 127) {
        buff[j]='.';
        count ++;
      };
    };
    buff[j]=0;
    min_count = min(min_count, count);

    printf("%s%s", buff, seperator);
  };

  printf("\n");
  return min_count;
};

int main(int argc, char **argv)
{
  int c,blocksize=4*1024;
  int columnwidth=20;
  int rows=2;
  int disks=0;
  char mode = 'r';
  char *stream_name = NULL;
  char *driver = AFF4_ZIP_VOLUME;
  int verify = 0;
  char *map_description = NULL;
  struct map_description *map = NULL;
  char *output = NULL;
  ZipFile volume = NULL;
  uint64_t start_block = 0;
  uint64_t disk_offset = 0;

  AFF4_DEBUG_LEVEL = 0;

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
      {"blocksize\0"
       "Blocksize to use",1,0,'b'},
      {"driver\0"
       "Which driver to use - 'directory' or 'volume' (Zip archive, default)", 1, 0, 'd'},
      {"output\0"
       "Create the output volume on this file or URL (using webdav)", 1, 0, 'o'},
      {"map\0"
       "map specification to use", 1, 0, 'm'},
      {"skip\0"
       "Skip this many blocks before starting", 1, 0, 's'},
      {"disk_offset\0"
       "Skip this many bytes from the start of each disk", 1, 0, 'd'},
      {"rows\0"
       "Number of rows in each block to print",1,0,'r'},
      {"write\0"
       "Write mode - actually create the map object with the specified name (if not specified just examine the raid)", 1, 0, 'w'},
      {0, 0, 0, 0}
    };

    c = getopt_long(argc, argv, generate_short_optargs(long_options),
		    long_options, &option_index);
    if (c == -1)
      break;
    switch (c) {

    case 'd':
      disk_offset = parse_int(optarg);
      break;

    case 'v':
      AFF4_DEBUG_LEVEL++;
      break;

    case 's':
      start_block = parse_int(optarg);
      break;

    case 'b':
      blocksize = parse_int(optarg);
      break;

    case 'm':
      map_description = optarg;
      break;

    case 'V':
      verify = 1;
      break;

    case '?':
    case 'h':
      printf("%s - an AFF4 general purpose imager.\n", argv[0]);
      print_help(long_options);
      exit(0);

    case 'r':
      rows = parse_int(optarg);
      break;

    case 'o':
      output = optarg;
      volume = open_volume(output);
      if(volume)
        CALL(oracle, cache_return, (AFFObject)volume);

      break;

    case 'w':
      mode = 'w';
      stream_name = optarg;
      break;

    default:
      printf("?? getopt returned character code 0%o ??\n", c);
    }
  }

  if(optind < argc) {
    int number_of_disks = argc - optind;
    FileLikeObject disks[optind - argc];
    int i;
    RDFURN urn = new_RDFURN(NULL);
    int period = 0;
    uint64_t block = start_block;

    for(i=0; i<number_of_disks; i++) {
      CALL(urn, set, argv[i+optind]);

      disks[i] = (FileLikeObject)CALL(oracle, open, urn, 'r');
      if(!disks[i]) {
        // Maybe its specified relative to the volume
        if(volume) {
          CALL(urn, set, URNOF(volume)->value);
          CALL(urn, add, argv[i+optind]);
          disks[i] = (FileLikeObject)CALL(oracle, open, urn, 'r');
        };
      };

      if(!disks[i]){
        goto exit;
      };

      printf("Position %u: %s\n", i, urn->value);
    };

    if(map_description) {
      map = parse_map(map_description, number_of_disks, &period,        \
                      blocksize, disks);
      if(!map->description) goto exit;
    };

    if(output && mode=='w') {
      make_map_stream(driver, map, output, stream_name);
      goto exit;
    };

    printf("Blocksize %u\n", blocksize);
    while(1) {
      for(i=0; i<number_of_disks; i++) {
        CALL(disks[i], seek, blocksize * block + disk_offset, SEEK_SET);
      };


      seperator = "  | ";
      for(i=0; i<rows; i++) {
        if(print_row(disks, number_of_disks, columnwidth)==0)
          seperator = " *| ";
      };

      for(i=0; i<number_of_disks; i++)
        printf("%*s%s", columnwidth, "", seperator);
      printf("\n");

      if(period > 0 && map->description) {
        for(i=0; i<number_of_disks; i++){
          char buff[BUFF_SIZE];

          snprintf(buff, BUFF_SIZE, "Slot %lld (%d)", 
                   block % period,
                   map->description[block % period][i]);
          printf("%-*s%s", columnwidth, buff, seperator);
        };
      } else {
        for(i=0; i<number_of_disks; i++)
          printf("Block %-*lld%s", columnwidth-6, block, seperator);
      };
      printf("\n");

      for(i=0; i<number_of_disks; i++)
        printf("%*s%s", columnwidth, "", seperator);

      for(i=0; i<number_of_disks; i++) {
        CALL(disks[i], seek, (block + 1) * blocksize - rows * columnwidth + disk_offset, SEEK_SET);
      };

      printf("\n");
      seperator = "  | ";
      for(i=0; i<rows; i++) {
        if(print_row(disks, number_of_disks, columnwidth)==0)
          seperator = " *| ";
      };

      printf("\n----- Offset %llu (Block %llu) -------\n\n", 
             disks[0]->readptr, block);

      block++;
    };
  };

 exit:
  PrintError();
  exit(EXIT_SUCCESS);
}
