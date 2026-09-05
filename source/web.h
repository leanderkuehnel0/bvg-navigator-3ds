#ifndef BVG_WEB_H
#define BVG_WEB_H

#include <3ds.h>
#include <stddef.h>

/* Performs a HTTPS GET. Returns a malloc()'d, NUL-terminated body on
   success (status 200), or NULL on any failure.
   If status_out is non-NULL it receives the HTTP status code. */
char* http_get(const char* url, u32* status_out);

#endif