/*
** talloc_test.c
** 
** Made by (mic)
** Login   <mic@laptop>
** 
** Started on  Thu Jan 14 19:54:21 2010 mic
** Last update Thu Jan 14 22:24:45 2010 mic
*/

#include "talloc.h"

int main() {
  char *a,*b,*c, *d;

  talloc_set_log_fn(printf);
  talloc_enable_leak_report_full();

  a = talloc_strdup(NULL, "A");
  b = talloc_strdup(a, "B");
  c = talloc_strdup(NULL, "C");
  d = talloc_strdup(c, "D");

  // Add a reference to C from b
  talloc_increase_ref_count(b);

  talloc_free(a);
  talloc_free(b);
  talloc_free(c);

  return 0;
};
