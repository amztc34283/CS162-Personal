/*
 * mm_alloc.h
 *
 * Exports a clone of the interface documented in "man 3 malloc".
 */

#pragma once

#ifndef _malloc_H_
#define _malloc_H_

#include <stdlib.h>
#include <stdbool.h>

void* mm_malloc(size_t size);
void* mm_realloc(void* ptr, size_t size);
void mm_free(void* ptr);

//TODO: Add any implementation details you might need to this file
typedef struct metadata
{
  size_t size;
  bool free;
  struct metadata *prev;
  struct metadata *next;
  char contents[0];
} metadata_t;

#endif
