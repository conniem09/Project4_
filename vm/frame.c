/* frame.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

*/
#include "vm/frame.h"
#include <stdio.h>
#include "threads/malloc.h"

/* Total number of pages in user pool. */
#define NUM_USR_FRAMES 383

void frame_table_init (void)
{
  frame_table = malloc (NUM_USR_FRAMES * sizeof (struct frame_table_entry));
}












/* For debugging. Eventually. At some point. Maybe? 
   I'm tired of debugging. */
void print_frame_table (void)
{
  struct frame_table_entry *fte = *frame_table;
  int frame_table_index = 0;

  printf ("\nFrame table entries:\n");
  while (fte != NULL)
    {
      printf("\tframe table entry %d\n", frame_table_index);
      printf("pd: %p\n", fte->pd);
      printf("upage: %p\n", fte->upage);
      printf("owner: %p\n", fte->owner);
      printf("pinned: %d\n", fte->pinned);
      frame_table_index++;
      fte = frame_table[frame_table_index];
    }
}
