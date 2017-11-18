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
#include "filesys/off_t.h"

/* Supplementary page table entry. */
struct supp_pte
{
  void *upage;                 /* User page. */
  bool in_swap;                /* Whether page is in swap. */
  bool in_filesys;             /* Whether page is in filesys, not in frame. */
  bool stack_page;             /* Whether page is a stack page. */
  size_t swap_loc;           /* Location in swap. */
  size_t read_bytes;           /* Non-zero byters */
  struct file *file;           /* File the page was loaded from. */
  off_t ofs;                   /* Offset in file. */
  bool writable;
  int frame_index;             /* Index of the frame that the page is in. */
  struct hash_elem supp_elem;  /* Hash elem for supp_page_table. */

  void *kpage;
};

unsigned pt_hash_func (const struct hash_elem *element, void *aux);
bool pt_less_func (const struct hash_elem *a, const struct hash_elem *b, 
	void *aux);
struct supp_pte *spte_lookup (void *upage);
void destroy_spt (void);
void pte_free (struct hash_elem *e, void *aux);

void spte_create (void *upage, void *kpage, bool in_swap, bool in_filesys, 
	              bool stack_page, uint32_t swap_loc, struct file *file, 
	              off_t ofs, size_t read_bytes, bool writable);

bool set_page_filesys (struct supp_pte *spte, void *upage, void *kpage);
bool set_page_swap (struct supp_pte *spte, void *upage, void *kpage);
void print_spte (struct supp_pte *spte);

#endif /* vm/page.h */
