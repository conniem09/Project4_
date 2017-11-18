/* swap.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

*/

#include "vm/swap.h"
#include "lib/debug.h"
#include <bitmap.h>

struct bitmap *swap_table;

void swap_table_init (size_t bit_cnt)
{
  /* Do some error checking for bitmap_create */
  swap_table = bitmap_create (bit_cnt);
}

/* Scans swap_table bitmap for free spot of PGSIZE in the swap partition.
   Returns the index of the free spot. 
   If swap partition is full, kernel is panicked. */
size_t find_free_swap_loc (void)
{
  size_t loc = bitmap_scan_and_flip (swap_table, 0, 1, 0);

  if (loc == BITMAP_ERROR)
    PANIC("Swap is out of swap");
  return loc; 
}

/* Reads page from swap and writes into buffer. 
   Returns the buffer. */
void *swap_read_page (size_t sector, void *buffer)
{
  struct block *swap_block = block_get_role (BLOCK_SWAP);
  block_read (swap_block, (block_sector_t) sector, buffer);
  return buffer;
}
