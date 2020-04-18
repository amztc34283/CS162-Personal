#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
//#include "userprog/pagedir.c"
#include "userprog/pagedir.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void
syscall_exit (int status)
{
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  thread_exit ();
}

/*
 * This does not check that the buffer consists of only mapped pages; it merely
 * checks the buffer exists entirely below PHYS_BASE.
 */
static void
validate_buffer_in_user_region (const void* buffer, size_t length)
{
  uintptr_t delta = PHYS_BASE - buffer;
  if (!is_user_vaddr (buffer) || length > delta)
    syscall_exit (-1);
}

/*
 * This does not check that the string consists of only mapped pages; it merely
 * checks the string exists entirely below PHYS_BASE.
 */
static void
validate_string_in_user_region (const char* string)
{
  uintptr_t delta = PHYS_BASE - (const void*) string;
  if (!is_user_vaddr (string) || strnlen (string, delta) == delta)
    syscall_exit (-1);
}


static int
syscall_open (const char* filename)
{
  struct thread* t = thread_current ();
  if (t->open_file != NULL)
    return -1;

  t->open_file = filesys_open (filename);
  if (t->open_file == NULL)
    return -1;

  return 2;
}

static int
syscall_write (int fd, void* buffer, unsigned size)
{
  struct thread* t = thread_current ();
  if (fd == STDOUT_FILENO)
    {
      putbuf (buffer, size);
      return size;
    }
  else if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int) file_write (t->open_file, buffer, size);
}

static int
syscall_read (int fd, void* buffer, unsigned size)
{
  struct thread* t = thread_current ();
  if (fd != 2 || t->open_file == NULL)
    return -1;

  return (int) file_read (t->open_file, buffer, size);
}

static void
syscall_close (int fd)
{
  struct thread* t = thread_current ();
  if (fd == 2 && t->open_file != NULL)
    {
      file_close (t->open_file);
      t->open_file = NULL;
    }
}

static uint32_t
syscall_sbrk (intptr_t increment)
{
  struct thread* t = thread_current();
  intptr_t sbrk = t->sbrk;
  uint32_t heap_base = t->heap_base;

  // TODO handle weird corner case that the address is already page aligned
  if (increment > 0) {
    uint32_t desire = heap_base + sbrk + increment;
    uint32_t num_pg_alloc = ((int) pg_round_up(desire) - (int) sbrk - heap_base) / PGSIZE;
    for (int i = 0; i < num_pg_alloc; i++) {
      // p is the kernel virtual address
      uint32_t *p = palloc_get_page(PAL_ZERO | PAL_USER);
      if (p == NULL) {
        t->sbrk = sbrk;
        // UNDO all palloc page
        while(i != 0) {
          uint32_t *p_remove = pagedir_get_page(t->pagedir, pg_round_up(heap_base + sbrk) + PGSIZE * (i - 1));
          pagedir_clear_page(t->pagedir, pg_round_up(heap_base + sbrk) + PGSIZE * (i - 1));
          palloc_free_page(p_remove);
          i--;
        }
        return -1;
      }
      // Corner Case: when sbrk is zero, that page also has to be allocated.
      if(!pagedir_set_page(t->pagedir, pg_round_up(heap_base + sbrk) + PGSIZE * i, p, true)) {
        t->sbrk = sbrk;
        // TODO: might need to something similar above
        return -1;
      }
    }
    t->sbrk = desire - heap_base;
  } else if (increment < 0) {
    uint32_t desire = heap_base + sbrk + increment;
    uint32_t num_pg_alloc = ((int) pg_round_up(sbrk + heap_base) - (int) desire) / PGSIZE;
    for (int i = 0; i < num_pg_alloc; i++) {
      // p is the kernel virtual address
      // UNDO all palloc page
      uint32_t *p_remove = pagedir_get_page(t->pagedir, pg_round_up(heap_base + sbrk) - PGSIZE * (i + 1));
      pagedir_clear_page(t->pagedir, pg_round_up(heap_base + sbrk) - PGSIZE * (i + 1));
      palloc_free_page(p_remove);
    }
    t->sbrk = desire - heap_base;
  }

  // intptr_t is signed integer
  return heap_base + sbrk;
}

static void
syscall_handler (struct intr_frame *f)
{
  uint32_t* args = (uint32_t*) f->esp;
  struct thread* t = thread_current ();
  t->in_syscall = true;

  validate_buffer_in_user_region (args, sizeof(uint32_t));
  switch (args[0])
    {
    case SYS_EXIT:
      validate_buffer_in_user_region (&args[1], sizeof(uint32_t));
      syscall_exit ((int) args[1]);
      break;

    case SYS_OPEN:
      validate_buffer_in_user_region (&args[1], sizeof(uint32_t));
      validate_string_in_user_region ((char*) args[1]);
      f->eax = (uint32_t) syscall_open ((char*) args[1]);
      break;

    case SYS_WRITE:
      validate_buffer_in_user_region (&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region ((void*) args[2], (unsigned) args[3]);
      f->eax = (uint32_t) syscall_write ((int) args[1], (void*) args[2], (unsigned) args[3]);
      break;

    case SYS_READ:
      validate_buffer_in_user_region (&args[1], 3 * sizeof(uint32_t));
      validate_buffer_in_user_region ((void*) args[2], (unsigned) args[3]);
      f->eax = (uint32_t) syscall_read ((int) args[1], (void*) args[2], (unsigned) args[3]);
      break;

    case SYS_CLOSE:
      validate_buffer_in_user_region (&args[1], sizeof(uint32_t));
      syscall_close ((int) args[1]);
      break;

    case SYS_SBRK:
      f->eax = (uint32_t) syscall_sbrk((intptr_t) args[1]);
      break;

    default:
      printf ("Unimplemented system call: %d\n", (int) args[0]);
      break;
    }

  t->in_syscall = false;
}
