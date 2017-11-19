/* frame.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

*/
#include "vm/frame.h"
#include <stdio.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"

struct lock ft_lock;

void frame_table_init (void)
{
  lock_init (&ft_lock);
  frame_table = malloc (NUM_USR_FRAMES * sizeof (struct frame_table_entry));
}

/* Create new frame table entry and adds it to the frame table. */
void create_fte (void *upage, void *kpage)
{ 
  struct frame_table_entry *new_entry;

  new_entry = malloc (sizeof (struct frame_table_entry));
  new_entry->owner = thread_current ();
  new_entry->pd = thread_current ()->pagedir;
  new_entry->upage = upage;
  new_entry->pinned = false;

  lock_acquire (&ft_lock);
  frame_table[get_ft_index (kpage)] = new_entry;
  lock_release (&ft_lock);
}

/* Remove all of a dying process's frame table entries. */
void remove_all_fte (struct thread *dying)
{
  lock_acquire (&ft_lock);
  int ft_index;
  for (ft_index = 0; ft_index < NUM_USR_FRAMES; ft_index++)
    {
      if (frame_table[ft_index] != NULL) 
        {
          if (frame_table[ft_index]->owner == dying)
            {
              free (frame_table[ft_index]);
              frame_table[ft_index] = NULL;
            }
        }
    }
  lock_release (&ft_lock);
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
