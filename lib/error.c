/*
** error.c
** 
** Made by (mic)
** Login   <mic@laptop>
** 
** Started on  Mon Mar 15 20:45:09 2010 mic
** Last update Sun May 12 01:17:25 2002 Speed Blue
*/

#include <pthread.h>
#include <class.h>
#include <string.h>
#include "aff4_errors.h"

#define ERROR_BUFF_SIZE 10240

static char error_buffer[ERROR_BUFF_SIZE];
static int error_type;


DLL_PUBLIC void *aff4_raise_errors(int t, char *reason, ...) {
  char tmp[ERROR_BUFF_SIZE];

  if(reason) {
    va_list ap;
    va_start(ap, reason);

    vsnprintf(tmp, ERROR_BUFF_SIZE-1, reason,ap);
    tmp[ERROR_BUFF_SIZE-1]=0;
    va_end(ap);
  };

  if(error_type == EZero) {
    *error_buffer = 0;

    //update the error type
    error_type = t;
  } else {
    strncat(error_buffer, "\n", ERROR_BUFF_SIZE -1 );
  };

  strncat(error_buffer, tmp, ERROR_BUFF_SIZE-1);

  return NULL;
};

DLL_PUBLIC int *aff4_get_current_error(char **error_buffer) {

  return &error_type;
};


/** Initialise the error subsystem */
AFF4_MODULE_INIT(A000_error) {

};

