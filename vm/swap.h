/* swap.h

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

*/

#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "vm/frame.h"

#define SECTORS_PER_PAGE PGSIZE / BLOCK_SECTOR_SIZE 

void swap_table_init (size_t bit_cnt);
uint32_t find_free_swap_loc (void);
void *swap_read_page (size_t swap_loc, void *kpage);
void *swap_write_page (size_t swap_loc, void *kpage);
bool check_and_write_swap (struct frame_table_entry *victim);

#endif /* vm/swap.h */
