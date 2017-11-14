/* page.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

*/
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <hash.h>

/* Supplementary page table entry. */
struct supp_pte
{
  void *upage;
  bool in_swap;
  bool in_filesys;
  bool stack_page;
  uint32_t swap_loc;
  uint32_t filesys_loc;
  int frame_index;
  struct hash_elem supp_elem;
};

unsigned pt_hash_func (const struct hash_elem *element, void *aux);
bool pt_less_func (const struct hash_elem *a, const struct hash_elem *b, 
	void *aux);

#endif /* vm/page.h */
