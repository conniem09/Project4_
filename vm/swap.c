/* swap.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

*/

#include "vm/swap.h"
#include <bitmap.h>

struct bitmap *swap_table;

void swap_table_init (size_t bit_cnt)
{
  /* Do some error checking for bitmap_create */
  swap_table = bitmap_create (bit_cnt);
}
