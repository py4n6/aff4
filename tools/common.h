#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "misc.h"

void print_help(struct option *opts);
char *generate_short_optargs(struct option *opts);
