/* page.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

 */
#include "vm/page.h"
#include "userprog/pagedir.h"

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
  return ((pte_a->upage < pte_b->upage) ? false : true);
}

