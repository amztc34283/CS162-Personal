/*
 * mm_alloc.c
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

metadata_t *head;
metadata_t *tail;

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
  metadata_t *sub_block = begin->contents[size+sizeof(metadata_t)];
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
  while(head != NULL) {
    if (head->free && head->size >= size) {
      return head;
    }
    head = head->next;
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
      first_fit->size = size;
      first_fit->free = false;
      zero_fill(first_fit->contents, first_fit->size);
      return first_fit->contents;
    }
  }
  return NULL;
}

void* mm_realloc(void* ptr, size_t size)
{
  //TODO: Implement realloc

  return NULL;
}

void coalesce(metadata_t *ptr)
{

}

void mm_free(void* ptr)
{
  //TODO: Implement free
  // Go back to the metadata header
  if (ptr == NULL)
    return;
  ptr = ptr-sizeof(metadata_t);
  metadata_t *free_ptr = ptr;
  free_ptr->free = true;
  //TODO coalesce and zero-fill in malloc
}
