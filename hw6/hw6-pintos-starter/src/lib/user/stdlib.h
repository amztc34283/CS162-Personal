#include <stdbool.h>
#ifndef __LIB_USER_STDLIB_H
#define __LIB_USER_STDLIB_H

void* malloc (size_t size);
void free (void* ptr);
void* calloc (size_t nmemb, size_t size);
void* realloc (void* ptr, size_t size);

typedef struct metadata
{
  size_t size;
  bool free;
  struct metadata *prev;
  struct metadata *next;
  char contents[0];
} metadata_t;

#endif /* lib/user/stdlib.h */
