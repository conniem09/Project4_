/* frame.h 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

*/

#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdint.h>
#include <stdbool.h>

/* Frame table of frame table entries. */
struct frame_table_entry **frame_table;

/* Frame table entry. */
struct frame_table_entry 
{
  uint32_t *pd;
  void *upage;
  struct thread *owner;
  bool pinned;
};

void frame_table_init (void);

void print_frame_table (void);

#endif /* vm/frame.h */
