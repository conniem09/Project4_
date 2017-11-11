/* frame.c 

 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 

*/

struct frame_table_entry **frame_table;

struct frame_table_entry 
{
  uint32_t *pd;
  void *upage;
  struct thread *owner;
  bool pinned;
};

void frame_table_init ()
{
  
  

}