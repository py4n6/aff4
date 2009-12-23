/*
** aff4_http.h
** 
** Made by mic
** Login   <mic@laptop>
** 
** Started on  Wed Dec 23 22:31:11 2009 mic
** Last update Wed Dec 23 22:32:03 2009 mic
*/

#ifndef   	AFF4_HTTP_H_
# define   	AFF4_HTTP_H_

#include "aff4_objects.h"
#include <curl/curl.h>

// This is implemented in using libcurl
CLASS(HTTPObject, FileLikeObject)
// The socket
     CURL *curl;
     StringIO buffer;

     CURL *send_handle;
     StringIO send_buffer;
     int send_buffer_offset;

     CURLM *multi_handle;

     HTTPObject METHOD(HTTPObject, Con, char *url);
END_CLASS

#endif 	    /* !AFF4_HTTP_H_ */
