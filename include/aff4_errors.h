/*
** aff4_errors.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Sat Mar  6 20:54:25 2010 mic
** Last update Sat Mar  6 20:54:25 2010 mic
*/

#ifndef   	AFF4_ERRORS_H_
# define   	AFF4_ERRORS_H_

// Some helpful little things
#define ERROR_BUFFER_SIZE 1024

/** This is used for error reporting. This is similar to the way
    python does it, i.e. we set the error flag and return NULL.
*/
enum _error_type {
  EZero,EGeneric,EOverflow,EWarning,
  EUnderflow,EIOError, ENoMemory, EInvalidParameter, ERuntimeError, EKeyError
};

void *raise_errors(enum _error_type t, char *string,  ...);

/** We only set the error state if its not already set */
#define RaiseError(t, message, ...)                                     \
  if(aff4_error == EZero) {                                             \
    raise_errors(t, "%s: (%s:%d) " message, __FUNCTION__, __FILE__, __LINE__, ## __VA_ARGS__); \
  };

#define LogWarnings(format, ...)		\
  do {						\
    RaiseError(EWarning, format, ## __VA_ARGS__);	\
    PrintError();				\
  } while(0);
  
#define ClearError()				\
  do {aff4_error = EZero;} while(0);

#define PrintError()				\
  do {if(aff4_error) fprintf(stdout, "%s",__error_str); fflush(stdout); ClearError(); }while(0);

#define CheckError(error)			\
  (aff4_error == error)

#endif 	    /* !AFF4_ERRORS_H_ */
