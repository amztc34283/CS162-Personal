/*
 * mm_alloc.c
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static metadata_t *head;
static metadata_t *tail;

// This is for use after each call of sbrk
void new_mapped_region(metadata_t* begin, size_t size, metadata_t *prev)
{
  begin->size = size;
  begin->free = false;
  begin->prev = prev;
  begin->next = NULL;
  // set prev->next to begin if prev is not null
  if (prev != NULL)
    prev->next = begin;
}

void* get_content_addr(metadata_t *block)
{
  return block->contents;
}

void zero_fill(char *content, size_t size)
{
  for (int i = 0; i < size; i++) {
    *(content+i) = 0;
  }
}

void* split_large_block(metadata_t *begin, size_t size)
{
  metadata_t *sub_block = begin->contents+size;
  sub_block->size = begin->size-size-sizeof(metadata_t);
  sub_block->free = true;
  sub_block->prev = begin;
  sub_block->next = begin->next;
  begin->size = size;
  begin->free = false;
  begin->next = sub_block;
  zero_fill(begin->contents, begin->size);
  return get_content_addr(begin);
}

// Find the first fit
metadata_t *find_first_fit(size_t size)
{
  metadata_t *ptr = head;
  while(ptr != NULL) {
    if (ptr->free && ptr->size >= size) {
      return ptr;
    }
    ptr = ptr->next;
  }
  return NULL;
}

void* mm_malloc(size_t size)
{
  //TODO: Implement malloc
  // Search mapped memory for a vacant spot
  // If not found, try to get the memory allocated
  // Successful: init a list or append to the list with metadata
  // Failed: cannot allocate the requested size 
  if (size == 0)
    return NULL;

  if (head == NULL) {
    metadata_t *begin;
    // extend break by the requested size and the size of the metadata
    if((void *) (begin = sbrk(size + sizeof(metadata_t))) == (void *) -1) {
      perror("sbrk failed.");
      return NULL;
    }
    new_mapped_region(begin, size, head);
    head = begin;
    tail = begin;
    zero_fill(begin->contents, begin->size);
    return get_content_addr(begin);
  } else { // it is not the first time to map memory.
    // search existing metadata_t for spot
    metadata_t *first_fit = find_first_fit(size);
    // if there is no block found -> allocate new block using sbrk
    if (first_fit == NULL) {
      metadata_t *begin;
      // extend break by the requested size and the size of the metadata
      if((void *) (begin = sbrk(size + sizeof(metadata_t))) == (void *) -1) {
        perror("sbrk failed.");
        return NULL;
      }
      new_mapped_region(begin, size, tail);
      tail = begin;
      zero_fill(begin->contents, begin->size);
      return get_content_addr(begin);
    }
          
    // if first_fit is sufficiently big enough
    if (first_fit->size >= size+sizeof(metadata_t)) {
      return split_large_block(first_fit, size);
    } else if (first_fit->size >= size) {  // if first_fit is barely big enough
      // first_fit->size = size; we may not need to set size
      first_fit->free = false;
      zero_fill(first_fit->contents, first_fit->size);
      return get_content_addr(first_fit);
    }
  }
  return NULL;
}

void coalesce(metadata_t *ptr)
{
  if (ptr == NULL)
    return;

  metadata_t *head = ptr;
  metadata_t *tail = ptr;

  while(head->prev != NULL && head->prev->free) {
    head = head->prev;
  }

  while(tail->next != NULL && tail->next->free) {
    tail = tail->next;
  }

  metadata_t *res = head;

  // handle the case of no adjacent free block
  if (head == ptr && tail == ptr)
    return;

  size_t new_size = 0;
  while(head != tail) {
    new_size += head->size + sizeof(metadata_t);
    head = head->next;
  }
  new_size += tail->size;
  res->size = new_size;
  res->next = tail->next;
}

void mm_free(void* ptr)
{
  //TODO: Implement free
  // Go back to the metadata header
  if (ptr == NULL)
    return;
  metadata_t *free_ptr = ptr-sizeof(metadata_t);
  free_ptr->free = true;
  coalesce(free_ptr);
}

void* mm_realloc(void* ptr, size_t size)
{
  //TODO: Implement realloc
  if (ptr != NULL && size == 0) {
    mm_free(ptr);
    return NULL;
  } else if (ptr == NULL) {
    return mm_malloc(size);
  }
  metadata_t *realloc_ptr = ptr-sizeof(metadata_t);
  size_t contents_size = realloc_ptr->size;
  char *contents = realloc_ptr->contents;
  mm_free(ptr);
  void* new_ptr = mm_malloc(size);
  if (new_ptr == NULL)
    return NULL;
  size_t final_size = size > contents_size ? contents_size: size;
  return memcpy(new_ptr, contents, final_size);
}
