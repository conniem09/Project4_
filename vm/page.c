/* page.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

 */
#include <string.h>
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include "threads/thread.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

/* Hash user virtual address (upage). */
unsigned 
pt_hash_func (const struct hash_elem *element, void *aux)
{
  struct supp_pte *spte = hash_entry (element, struct supp_pte, supp_elem);
  return hash_bytes (&spte->upage, sizeof (void *));
}

/* Compare supplementary page table entries by upage. */
bool 
pt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  struct supp_pte *pte_a = hash_entry (a, struct supp_pte, supp_elem);
  struct supp_pte *pte_b = hash_entry (b, struct supp_pte, supp_elem);
  return pte_a->upage < pte_b->upage;
}

/* Creates new supplementary page table entry. */
void 
spte_create (void *upage, void *kpage, bool in_swap, bool in_filesys, 
	              bool stack_page, uint32_t swap_loc, struct file *file, 
	              off_t ofs, size_t read_bytes, bool writable)
{
  //printf("\nSTART BLOCK 3\n");
  struct supp_pte *new_pte;
  new_pte = malloc (sizeof (struct supp_pte));
  new_pte->upage = upage;
  new_pte->in_swap = in_swap;
  new_pte->in_filesys = in_filesys;
  new_pte->stack_page = stack_page;
  new_pte->swap_loc = swap_loc;
  new_pte->read_bytes = read_bytes;
  new_pte->file = file;
  new_pte->ofs = ofs;
  new_pte->writable = writable;
  new_pte->kpage = kpage;
  new_pte->frame_index = get_ft_index (kpage);

  hash_insert (&thread_current ()->supp_page_table, &new_pte->supp_elem);

  /* Debugging. Remove later. */
  //print_spte (spte_lookup (upage));
  //printf("\nEND BLOCK 3\n");
}

struct supp_pte *
spte_lookup (void *upage)
{

  struct supp_pte spte;
  struct hash_elem *e;

  spte.upage = upage;
  e = hash_find (&thread_current ()->supp_page_table, &spte.supp_elem);
  return e != NULL ? hash_entry (e, struct supp_pte, supp_elem) : NULL;
}

/* Destroy thread's supplementary page table. */
void 
destroy_spt (struct thread *t)
{
  hash_apply (&t->supp_page_table, &pte_free);
}

/* Free a supplementary page table entry. */
void 
pte_free (struct hash_elem *e, void *aux)
{
  free (hash_entry (e, struct supp_pte, supp_elem));
}

/* Loads a page from the file system. Returns true on success.
   Returns false if allocation fails. */
bool
set_page_filesys (struct supp_pte *spte, void *upage, void *kpage)
{
  //printf("\nset_page_filesys() HERE!\n");
  /* There is data to be read from the file. */
  if (spte->file != NULL)
    {
      struct file *file = spte->file;
      off_t ofs = spte->ofs; 
      file_seek (file, ofs);

      lock_acquire (&filesys_lock);
      if (file_read (file, kpage, spte->read_bytes) != (int) spte->read_bytes)
        {
          palloc_free_page (kpage);
          lock_release (&filesys_lock);
          return false; 
        }
      lock_release (&filesys_lock);
    }
  create_fte (upage, kpage);
  memset (kpage + spte->read_bytes, 0, PGSIZE - spte->read_bytes);

  if (pagedir_set_page (thread_current ()->pagedir, upage, kpage, 
                        spte->writable)) 
    {
      create_fte (upage, kpage);
      spte->in_filesys = false;
      return true;
    }
  else
    {
      palloc_free_page (kpage);
      return false;
    }
}

/* Brings in a page from swap. Assumes there is already a free frame. 
   Returns true on success, false if allocation fails. */
bool 
set_page_swap (struct supp_pte *spte, void *upage, void *kpage)
{
  if (pagedir_set_page (thread_current ()->pagedir, upage, kpage, 
                        spte->writable)) 
    {
      kpage = swap_read_page (spte->swap_loc, kpage);
      create_fte (upage, kpage);
      spte->in_swap = false;
      return true;
    }
  else
    {
      palloc_free_page (kpage);
      return false;
    }
}

/* Allocates a new stack page in the case of stack growth.
   Returns true on success, false if allocation fails. */
bool
set_page_stack (void *upage, void *kpage)
{ 
  memset (kpage, 0, PGSIZE);
  spte_create (upage, kpage, false, false, true, 0, NULL, 0, 0, true);
  if (pagedir_set_page (thread_current ()->pagedir, upage, kpage, true)) 
    {
      create_fte (upage, kpage);
      struct supp_pte *spte = spte_lookup (upage);
      spte->stack_page = true;
      return true;
    }
  else
    {
      palloc_free_page (kpage);
      return false;
    }
}


void 
print_spte (struct supp_pte *spte)
{
  if (spte == NULL)
    {
      printf ("You done fucked up somehow.\n");
      return;
    }
  // printf ("upage: %p\n", spte->upage);
  // printf ("kpage: %p\n", spte->kpage);
  // printf ("in_swap: %d\n", spte->in_swap);
  // printf ("in_filesys: %d\n", spte->in_filesys);
  // printf ("stack_page: %d\n", spte->stack_page);
  // printf ("swap_loc: %d\n", spte->swap_loc);
  // printf ("read_bytes: %d\n", spte->read_bytes);
  // printf ("file: %p\n", spte->file);
  // printf ("ofs: %d\n", spte->ofs);
  // printf ("writable: %d\n\n", spte->writable);
}

