#ifndef BVG_WEB_H
#define BVG_WEB_H

#include <3ds.h>
#include <stddef.h>

/* Performs a HTTPS GET (following redirects, retrying transient 5xx).
   Returns a malloc()'d, NUL-terminated body on success (status 200), or
   NULL on any failure.
   If status_out is non-NULL it receives the HTTP status code (0 if the
   server never answered). If err_out is non-NULL it receives the first
   failing libctru Result code (0 if a plain HTTP error occurred). */
char* http_get(const char* url, u32* status_out, u32* err_out);

#endif