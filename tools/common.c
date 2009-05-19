#include "common.h"

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


char *generate_short_optargs(struct option *opts) {
  static char short_opts[BUFF_SIZE];
  int j,i;
  
  for(i=0, j=0; i<BUFF_SIZE && opts[j].name; j++) {
    if(opts[j].val) {
      short_opts[i]=opts[j].val;
      i++;
      if(opts[j].has_arg) {
	short_opts[i]=':';
	i++;
      };
    };
  };
  
  short_opts[i]=0;

  return short_opts;
};
