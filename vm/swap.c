/* swap.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

*/

#include "vm/swap.h"
#include "lib/debug.h"
#include <bitmap.h>
#include "userprog/pagedir.h"
#include "vm/page.h"

struct bitmap *swap_table;

void 
swap_table_init (size_t bit_cnt)
{
  /* Do some error checking for bitmap_create */
  swap_table = bitmap_create (bit_cnt);
}

/* Scans swap_table bitmap for free spot of PGSIZE in the swap partition,
   and flips the bit in swap_table to indicate the spot is not free anymore. 
   Returns the index of the free spot. 
   If swap partition is full, kernel is panicked. */
size_t 
find_free_swap_loc (void)
{
  size_t loc = bitmap_scan_and_flip (swap_table, 0, 1, false);

  if (loc == BITMAP_ERROR)
    PANIC("Swap is out of swap");
  return loc; 
}

/* Reads page from swap at location swap_loc, and writes into a frame. 
   Returns the buffer kpage. */
void *
swap_read_page (size_t swap_loc, void *kpage)
{
  int i;
  size_t sector = swap_loc * SECTORS_PER_PAGE;
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  
  for (i = 0; i < SECTORS_PER_PAGE; i++)
    {
      block_read (swap_block, (block_sector_t) sector + i, 
                  kpage + i * BLOCK_SECTOR_SIZE);
    }
  bitmap_set (swap_table, swap_loc, false);
  return kpage;
}

/* Writes kpage into swap at location swap_loc.
   Returns the buffer kpage. */
void *
swap_write_page (size_t swap_loc, void *kpage)
{
  int i;
  size_t sector = swap_loc * SECTORS_PER_PAGE;
  struct block *swap_block = block_get_role (BLOCK_SWAP);

  for (i = 0; i < SECTORS_PER_PAGE; i++)
    {
      block_write (swap_block, (block_sector_t) sector + i,
                   kpage + i * BLOCK_SECTOR_SIZE);
    }
  return kpage;
}

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
      size_t swap_loc = find_free_swap_loc ();
      swap_write_page (swap_loc, victim->kpage);
      pagedir_clear_page (victim->pd, victim->upage);
      spte_lookup (victim->upage)->in_swap = true;
      return true;
    }
  else 
    {
      pagedir_clear_page (victim->pd, victim->upage);
      spte_lookup (victim->upage)->in_filesys = true;
      return false;
    }
} 
