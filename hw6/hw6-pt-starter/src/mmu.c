#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "page.h"
#include "ram.h"


/* These macros may or may not be useful.
 * */


//#define PMD_PFN_MASK
//#define PTE_PFN_MASK
//#define PAGE_OFFSET_MASK
//
//#define vaddr_pgd(vaddr)
//#define vaddr_pmd(vaddr)
//#define vaddr_pte(vaddr)
//#define vaddr_off(vaddr)
//
//#define pfn_to_addr(pfn) (pfn << PAGE_SHIFT)


/* Translates the virtual address vaddr and stores the physical address in paddr.
 * If a page fault occurs, return a non-zero value, otherwise return 0 on a successful translation.
 * */

#define PFN_MASK 0x000ffffffffff000
#define PRESENT_MASK 0x0000000000000001
#define PDE_MASK 0x3fe00000
#define PTE_MASK 0x001ff000
#define OFFSET_MASK 0x00000fff

// Define PDPTE, PDE, PTE as all 8 bytes type
typedef struct PDPTE { char x[8]; } PDPTE;
typedef struct PDE { char x[8]; } PDE;
typedef struct PTE { char x[8]; } PTE;

int virt_to_phys(vaddr_ptr vaddr, paddr_ptr cr3, paddr_ptr *paddr) {
  // Naive Implementation.

  // possible bug on arithmetic shift
  uint32_t pdpte_idx = vaddr >> 30;
  uint32_t pde_idx = (vaddr & PDE_MASK) >> 21;
  uint32_t pte_idx = (vaddr & PTE_MASK) >> 12;

  uint32_t offset = (vaddr & OFFSET_MASK);

  // sizeof might have bugs
  PDPTE* lv1_table = (PDPTE *) malloc(4 * sizeof(PDPTE));
  PDE* lv2_table = (PDE *) malloc(512 * sizeof(PDE));
  PTE* lv3_table = (PTE *) malloc(512 * sizeof(PTE));
  // char* page_table = (char *) malloc(4096);

  // fetch page things from cr3
  ram_fetch(cr3, lv1_table, 4 * sizeof(PDPTE)); 
  // 1. No page fault, returns 0
  // Page fault only happens when the present bit is not set.
  PDPTE pdpte_val = lv1_table[pdpte_idx];
  uint64_t* pdpte_uint_val = (uint64_t *) malloc(8);
  memcpy(pdpte_uint_val, pdpte_val.x, 8);
  if ((*pdpte_uint_val & PRESENT_MASK) == 0)
    return 1;

  // look okay so far.
  // translate the page dir/num to a physical address
  // 64 bits = 12 free bits + 40 bits from page + 12 bits from offset in a page
  *pdpte_uint_val = *pdpte_uint_val & PFN_MASK;  
  ram_fetch((paddr_ptr) *pdpte_uint_val, lv2_table, 512 * sizeof(PDE));
  PDE pde_val = lv2_table[pde_idx];
  uint64_t* pde_uint_val = (uint64_t *) malloc(8);
  memcpy(pde_uint_val, pde_val.x, 8);
  if ((*pde_uint_val & PRESENT_MASK) == 0)
    return 1;

  // looks good?
  *pde_uint_val = *pde_uint_val & PFN_MASK;
  ram_fetch((paddr_ptr) *pde_uint_val, lv3_table, 512 * sizeof(PTE));
  PTE pte_val = lv3_table[pte_idx];
  uint64_t* pte_uint_val = (uint64_t *) malloc(8);
  memcpy(pte_uint_val, pte_val.x, 8);
  if ((*pte_uint_val & PRESENT_MASK) == 0)
    return 1;

  // looks good
  // use offset to traverse the final table
  // last bit of offset is never used in 64 bits address
  //TODO
  *pte_uint_val = *pte_uint_val & PFN_MASK;
  //ram_fetch((paddr_ptr) *pte_uint_val, page_table, 4096);
  //*paddr = page_table[offset];
  *paddr = *pte_uint_val | offset;
  return 0;
}

char *str_from_virt(vaddr_ptr vaddr, paddr_ptr cr3) {
  size_t buf_len = 1;
  char *buf = malloc(buf_len);
  char c = ' ';
  paddr_ptr paddr;

  for (int i=0; c; i++) {
    if(virt_to_phys(vaddr + i, cr3, &paddr)){
      printf("Page fault occured at address %p\n", (void *) vaddr + i);
      return (void *) 0;
    }

    ram_fetch(paddr, &c, 1);
    buf[i] = c;
    if (i + 1 >= buf_len) {
      buf_len <<= 1;
      buf = realloc(buf, buf_len);
    }
    buf[i + 1] = '\0';
  }
  return buf;
}

int main(int argc, char **argv) {

  if (argc != 4) {
    printf("Usage: ./mmu <mem_file> <cr3> <vaddr>\n");
    return 1;
  }

  paddr_ptr translated;

  ram_init();
  ram_load(argv[1]);

  paddr_ptr cr3 = strtol(argv[2], NULL, 0);
  vaddr_ptr vaddr = strtol(argv[3], NULL, 0);


  if(virt_to_phys(vaddr, cr3, &translated)){
    printf("Page fault occured at address %p\n", vaddr);
    exit(1);
  }

  char *str = str_from_virt(vaddr, cr3);
  printf("Virtual address %p translated to physical address %p\n", vaddr, translated);
  printf("String representation of data at virtual address %p: %s\n", vaddr, str);

  return 0;
}
