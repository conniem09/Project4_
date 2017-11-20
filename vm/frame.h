/* frame.h 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 11/19/17
*/

#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <stdbool.h>

/* Total number of pages in user pool. */
#define NUM_USR_FRAMES 367

/* Frame table of frame table entries. */
struct frame_table_entry **frame_table;

/* Frame table entry. */
struct frame_table_entry 
{
  struct thread *owner;        /* Owner of the frame. */
  uint32_t *pd;                /* Owner's page directory. */
  void *upage;                 
  void *kpage;
  bool pinned;
};

void frame_table_init (void);
void create_fte (void *upage, void *kpage);
void destroy_fte (int ft_index);
void remove_all_fte (struct thread *dying);

/* Functions for eviction. */
struct frame_table_entry *choose_victim (void);
void *frame_evict (void);

void print_frame_table (void);

#endif /* vm/frame.h */
