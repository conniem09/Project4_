/* page.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

 */
#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

unsigned pt_hash_func (const struct hash_elem *element, void *aux)
{
  return hash_bytes (hash_entry (element, struct supp_pte, supp_elem)->upage, 
    sizeof (void*));
}

bool pt_less_func (const struct hash_elem *a, const struct hash_elem *b, 
	void *aux)
{
  struct supp_pte *pte_a = hash_entry (a, struct supp_pte, supp_elem);
  struct supp_pte *pte_b = hash_entry (b, struct supp_pte, supp_elem);
  return ((pte_a->upage < pte_b->upage) ? true : false);
}

/* Creates new supplementary page table entry. */
void spte_create (void *upage, void *kpage, bool in_swap, bool in_filesys, 
	              bool stack_page, uint32_t swap_loc, struct file *file, 
	              off_t ofs)
{
  struct supp_pte *new_pte;
  new_pte = malloc (sizeof (struct supp_pte));
  new_pte->upage = upage;
  new_pte->in_swap = in_swap;
  new_pte->in_filesys = in_filesys;
  new_pte->stack_page = stack_page;
  new_pte->swap_loc = swap_loc;
  new_pte->file = file;
  new_pte->ofs = ofs;
  new_pte->frame_index = get_ft_index (kpage);

  hash_insert (&thread_current ()->supp_page_table, &new_pte->supp_elem);
}

/* Destroy thread's supplementary page table. */
void destroy_spt (void)
{
  hash_apply (&thread_current ()->supp_page_table, &pte_free);
}

/* Free a supplementary page table entry. */
void pte_free (struct hash_elem *e, void *aux)
{
  free (&hash_entry (e, struct supp_pte, supp_elem));
}
