/* frame.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 11/19/17
*/
#include "vm/frame.h"
#include <stdio.h>
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"

/* Global frame table lock. */
struct lock ft_lock;

/* "Clock pointer" - frame table index that the "clock hand"
   is "pointing" to for the frame table. 
   Used for page replacement algorithm. */
int clock_hand;

/* Initialize the global frame table. */
void 
frame_table_init (void)
{
  clock_hand = 0;
  lock_init (&ft_lock);
  frame_table = malloc (NUM_USR_FRAMES * sizeof (struct frame_table_entry));
  if (frame_table == NULL)
    exit_handler (-1);
}

/* Cindy driving */
/* Create new frame table entry and adds it to the frame table. */
void 
create_fte (void *upage, void *kpage)
{ 
  struct frame_table_entry *new_entry;

  new_entry = malloc (sizeof (struct frame_table_entry));
  if (new_entry == NULL)
    exit_handler (-1);
  new_entry->owner = thread_current ();
  new_entry->pd = thread_current ()->pagedir;
  new_entry->upage = upage;
  new_entry->kpage = kpage;
  new_entry->pinned = false;

  lock_acquire (&ft_lock);
  frame_table[get_ft_index (kpage)] = new_entry;
  lock_release (&ft_lock);
}
/* end Cindy driving */

/* Connie driving */
/* Frees a frame table entry and removes it from the
   frame table. */
void
destroy_fte (int ft_index)
{
  free (frame_table[ft_index]);
  frame_table[ft_index] = NULL;
}

/* Remove all of a dying process's frame table entries. */
void 
remove_all_fte (struct thread *dying)
{
  lock_acquire (&ft_lock);
  int ft_index;
  for (ft_index = 0; ft_index < NUM_USR_FRAMES; ft_index++)
    {
      if (frame_table[ft_index] != NULL) 
        {
          if (frame_table[ft_index]->owner == dying)
            destroy_fte (ft_index);
        }
    }
  lock_release (&ft_lock);
}
/* end of Connie's driving */

/* Zachary driving */
/* Choose a victim frame to evict using the clock page
   replacement algorithm.
   Returns victim frame table entry. */
struct frame_table_entry *
choose_victim (void)
{
  bool victim_found = false;
  struct frame_table_entry *victim = NULL;

  while (!victim_found)
    {
      struct frame_table_entry *fte = frame_table[clock_hand];
      if (fte != NULL)
        {
          bool accessed = pagedir_is_accessed (fte->pd, fte->upage);
          if (!accessed && !fte->pinned)
            {
              victim = fte;
              victim_found = true;
            }
          else
            {
              pagedir_set_accessed (fte->pd, fte->upage, 0);
              pagedir_set_accessed (fte->pd, fte->kpage, 0);
            }
        }
      clock_hand++;
      if (clock_hand >= NUM_USR_FRAMES)
        clock_hand = 0;
    }
  return victim;
}
/* end of Zachary driving */

/* Cindy driving */
/* Evicts a frame using the clock page replacement algorithm.
   Returns the kernel virtual address of the now free frame. */
void *
frame_evict (void)
{
  lock_acquire (&ft_lock);
  struct frame_table_entry *victim = choose_victim ();
  void *kpage = victim->kpage;
  struct thread *victim_owner = victim->owner;

  /* Prevent owner of evicted frame from atttempting to 
     reclaim the page during frame eviction. */
  if (!(victim_owner == thread_current ()))
    lock_acquire (&victim->owner->frame_access_lock);
  
  pagedir_clear_page (victim->pd, victim->upage);
  check_and_write_swap (victim);
  destroy_fte (get_ft_index (kpage));
  
  if (!(victim_owner == thread_current ()))
    lock_release (&victim->owner->frame_access_lock);
  lock_release (&ft_lock);
  return kpage;
}
/* end of Cindy driving */

/* Connie driving */
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
/* end of Connie driving */
