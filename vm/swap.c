/* swap.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 11/19/17
*/

#include "vm/swap.h"
#include "lib/debug.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/page.h"

/* Bitmap keeping track of free spots of size PGSIZE
   in the swap partition. */
struct bitmap *swap_table;

/* Block device for swap partition. */
struct block *swap_block;

/* Connie take the wheel */
void 
swap_table_init (size_t bit_cnt)
{
  swap_table = bitmap_create (bit_cnt);
  if (swap_table == NULL)
    exit_handler (-1);
  swap_block = block_get_role (BLOCK_SWAP);
  lock_init (&swap_lock);
}
/* end of Connie driving */

/* Scans swap_table bitmap for free spot of PGSIZE in the swap partition,
   and flips the bit in swap_table to indicate the spot is not free anymore. 
   Returns the index of the free spot. 
   If swap partition is full, kernel is panicked. */
size_t 
find_free_swap_loc (void)
{
  size_t loc = bitmap_scan_and_flip (swap_table, 0, 1, false);

  if (loc == BITMAP_ERROR)
    PANIC("Swap is out of space.");
  return loc; 
}

/* Cindy stepping into the driver's seat */
/* Reads page from swap at location swap_loc, and writes into a frame. 
   Returns the buffer kpage. */
void *
swap_read_page (size_t swap_loc, void *kpage)
{
  int i;
  size_t sector = swap_loc * SECTORS_PER_PAGE;
  
  for (i = 0; i < SECTORS_PER_PAGE; i++)
    {
      block_read (swap_block, (block_sector_t) sector + i, 
                  kpage + i * BLOCK_SECTOR_SIZE);
    }
  bitmap_set (swap_table, swap_loc, false);
  return kpage;
}
/* end of Cindy's reign */

/* start Zachary's driving */
/* Writes kpage into swap at location swap_loc.
   Returns the buffer kpage. */
void *
swap_write_page (size_t swap_loc, void *kpage)
{
  int i;
  size_t sector = swap_loc * SECTORS_PER_PAGE;

  for (i = 0; i < SECTORS_PER_PAGE; i++)
    {
      block_write (swap_block, (block_sector_t) sector + i,
                   kpage + i * BLOCK_SECTOR_SIZE);
    }
  return kpage;
}
/* end of Zachary driving */

/* Cindy, Connie and Zachary driving */
/* Checks if page indicated by frame table entry victim is dirty.
   If so, the page is written out to swap in preparation for
   eviction. If not, it is assumed the page is in the filesys. 
   Returns true if page was dirty and written to swap, 
   false otherwise.*/
bool
check_and_write_swap (struct frame_table_entry *victim)
{
  if (pagedir_is_dirty (victim->pd, victim->upage))
    {
      lock_acquire (&swap_lock);
      size_t swap_loc = find_free_swap_loc ();
      swap_write_page (swap_loc, victim->kpage);
      lock_release (&swap_lock);

      struct supp_pte *spte = spte_lookup (victim->upage, victim->owner);
      spte->in_swap = true;
      spte->swap_loc = swap_loc;
      return true;
    }
  else 
    {
      spte_lookup (victim->upage, victim->owner)->in_filesys = true;
      return false;
    }
} 
/* End trio driving */
