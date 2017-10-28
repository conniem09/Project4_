#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "pagedir.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "lib/kernel/list.h"


#define MAX_ARGS 3
#define BLOCK_SIZE 200
#define FD_START 2
#define CODE_SEG_START  0x08048000

/* Global lock for filesys. */
static struct lock filesys_lock;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  //printf("esp at top of syscall_handler(): %p\n", (int *) f->esp);
  //printf ("syscall number: %d\n", *((int *) f->esp));
  validate_pointer ((const void *) f->esp);
  int arguments[MAX_ARGS];

  switch (*((int *) f->esp)) 
    {
      case SYS_HALT :
        halt_handler ();
        break;
      case SYS_EXIT :
        get_arg (arguments, f->esp, 1);
        exit_handler ((int) arguments[0]);
        f->eax = (int) arguments[0];
        break;
      case SYS_EXEC :
        get_arg (arguments, f->esp, 1);
        validate_pointer ((const void *) arguments[0]);
        f->eax = exec_handler ((const char *) arguments[0]);
        break;
      case SYS_WAIT :
        get_arg (arguments, f->esp, 1);
        f->eax = wait_handler ((int) arguments[0]);
        break;
      case SYS_CREATE :
        get_arg (arguments, f->esp, 2);
        validate_pointer ((const void *) arguments[0]);
        f->eax = create_handler ((const char *) arguments[0], 
                                  ((unsigned) arguments[1]));
        break;
      case SYS_REMOVE :
        get_arg (arguments, f->esp, 1);
        validate_pointer ((const void *) arguments[0]);
        f->eax = remove_handler ((const char *) arguments[0]);
        break;
      case SYS_OPEN :
        get_arg (arguments, f->esp, 1);
        validate_pointer ((const void *) arguments[0]);
        f->eax = open_handler ((const char *) arguments[0]);
        break;
      case SYS_FILESIZE :
        get_arg (arguments, f->esp, 1);
        f->eax = filesize_handler ((int) arguments[0]);
        break;
      case SYS_READ :
        get_arg (arguments, f->esp, 3);
        validate_buffer ((const void *) arguments[1], arguments[2]);
        f-> eax = read_handler (((int) arguments[0]), (void *) arguments[1], 
                     ((unsigned) arguments[2]));
        break;
      case SYS_WRITE :
        //printf("Here SYS_WRITE !\n");
        get_arg (arguments, f->esp, 3);
        validate_buffer ((const void *) arguments[1], arguments[2]);
        f->eax = write_handler (((int) arguments[0]), 
                                 ((const void *) arguments[1]), 
                                 ((unsigned) arguments[2]));
        break;
      case SYS_SEEK :
        get_arg (arguments, f->esp, 2);
        seek_handler (((int) arguments[0]), ((unsigned) arguments[1]));
        break;
      case SYS_TELL :
        get_arg (arguments, f->esp, 1);
        f->eax = tell_handler ((int) (arguments[0]));
        break;
      case SYS_CLOSE :
        get_arg (arguments, f->esp, 1);
        close_handler ((int) (arguments[0]));
        break;
    }
}

/* Retrieves system call argumentss from the user stack. 
   Returns arguments in an array of strings. */
void 
get_arg (int arguments[], void *esp, int num_arg)
{
  int *local_esp = (int *) esp;
  int i; 

  for (i = 0; i < num_arg; i++)
   {
     local_esp++;
     //printf("checking esp for arg %d:\t", i);
     validate_pointer ((const void*) local_esp);
     arguments[i] = *local_esp;
   }
}

/* Terminates Pintos. */
void 
halt_handler (void)
{
  shutdown_power_off ();
}

/* Terminates the current user program, returning status 
   to the kernel. */
void 
exit_handler (int status)
{
  struct thread *cur = thread_current ();
  
  //cur->exit_status = status;

  printf ("%s: exit(%d)\n", cur->execu_name, status);
  process_exit ();
  
  //printf("PARENT DURING EXIT: %p\n", cur->parent);
  if (cur->parent != NULL)
    {
      //printf ("Non-null parent here.\n");
      sema_down (&cur->child_exit_sema);
      cur->parent->child_exit_status = status;
      list_remove (&cur->child_elem);
      sema_up (&cur->parent->parent_wait_sema);
    }

  lock_acquire(&filesys_lock);
  struct file *my_ex = filesys_open(cur->execu_name);
  file_allow_write(my_ex);
  file_close(my_ex);
  lock_release(&filesys_lock);
  
  thread_exit ();
}

/* Runs the executable whose name is given in cmd_line.
   Returns the program's PID. If the program cannot 
   load or run for any reason, returns -1. */
pid_t 
exec_handler (const char *cmd_line)
{

  // Check that file exists
  char local_copy[15];
  char *local_pointer = local_copy;
  strlcpy (local_pointer, cmd_line, sizeof(local_copy));
  char *token = strtok_r (local_pointer, " ", &local_pointer);
  const char *file_name = token;

  lock_acquire (&filesys_lock);
  struct file *file = filesys_open(file_name);
  if (file == NULL)
    {
      file_close(file);
      lock_release (&filesys_lock);
      return -1;
    }
  lock_release (&filesys_lock);
  file_close(file);

  pid_t pid = process_execute (cmd_line);

  if (pid == TID_ERROR)
    return -1;

  lock_acquire(&filesys_lock);
  file_deny_write(file);
  lock_release(&filesys_lock);

  return pid;

}

/* Waits for a child process PID and returns child's exit
   status. Returns -1 if child process has been terminated 
   by the kernel.

   Fails and returns -1 if 1) pid does not refer to a direct 
   child of the calling process, or 2) calling process has 
   already called wait() on child PID. */
int 
wait_handler (pid_t pid)
{
  process_wait (pid);
}

/* Creates a new file called FILE initially INITIAL_SIZE bytes
   in size. Returns true if successful, false otherwise. */
bool 
create_handler (const char *file, unsigned initial_size)
{
  lock_acquire (&filesys_lock);
  bool created = filesys_create (file, (off_t) initial_size);
  lock_release (&filesys_lock);
  return created;
}

/* Deletes the file called FILE. Returns true if successful,
   false otherwise. A file may be removed regardless of whether
   it is open or closed, and removing an open file does not 
   close it. */
bool 
remove_handler (const char *file)
{
  lock_acquire (&filesys_lock);
  bool removed = filesys_remove (file);
  lock_release (&filesys_lock);
  return removed;
}

/* Opens the file called file. Returns a file descriptor fd or
   -1 if the file could not be opened. Each open() of a single
   file returns a new file descriptor. */
int 
open_handler (const char *file)
{
  int fd_index;
  struct thread *cur = thread_current ();

  lock_acquire (&filesys_lock);
  
  for (fd_index = FD_START; fd_index < MAX_FD_COUNT; fd_index++)
    {
      if (cur->open_files[fd_index] == NULL)
        {
          cur->open_files[fd_index] = filesys_open (file);
          if (cur->open_files[fd_index] != NULL)
            {
              cur->open_files[fd_index] = filesys_open (file);
              lock_release (&filesys_lock);
              return fd_index;
            }
        }
    }

  // int fd_index;
  // for (fd_index = FD_START; fd_index < MAX_FD_COUNT; fd_index++)
  //   {
  //     if (file_associated_tids[fd_index] == -1)
  //       {
  //         open_files[fd_index] = filesys_open (file);
  //         if (open_files[fd_index] == NULL)
  //           break;
  //         file_associated_tids[fd_index] = thread_current ()->tid;
  //         lock_release (&filesys_lock);
  //         return fd_index;
  //       }
  //   }
  lock_release (&filesys_lock);
  return -1;
}

/* Returns the size, in bytes, of the file open as fd. 
   If invalid id, returns -1. */
int 
filesize_handler (int fd) 
{
  if (!valid_fd (fd))
    return -1;

  lock_acquire (&filesys_lock);
  int size = file_length (thread_current ()->open_files[fd]);
  lock_release (&filesys_lock);
  return size;
}

/* Reads size bytes from the file open as fd into buffer. 
   Returns the number of bytes actually read (0 at end of file),
   or -1 if the file could not be read or fd is invalid. */
int 
read_handler (int fd, void *buffer, unsigned size)
{
  int size_read;
  if (fd == STDIN_FILENO)
    {
      size_read = 0;
      while ((unsigned) size_read < size) 
        {
          input_getc();
          size_read++;
        }
      return size_read;
    }

  if (!valid_fd (fd))
    return -1;

  struct file *file = thread_current ()->open_files[fd]
  if (file == NULL)
    return -1;

  lock_acquire (&filesys_lock);
  size_read = file_read (file, buffer, (off_t) size);
  lock_release (&filesys_lock);
  return size_read;
}

/* Write size bytes from buffer to the open file fd. Returns
   the number of bytes actually written. 

   fd 1 writes to the console. 

   If invalid id, returns -1. */
int 
write_handler (int fd, const void *buffer, unsigned size)
{
  //printf("Here write_handler !\n");
  if (fd == STDOUT_FILENO) 
    {
      int blocks = 0;
      int remaining = size;
      while (remaining >= BLOCK_SIZE)
      {
        putbuf (buffer + (blocks * BLOCK_SIZE), BLOCK_SIZE);
        remaining -= BLOCK_SIZE;
        blocks++;
      }
      putbuf (buffer + (blocks * BLOCK_SIZE), remaining);
      return size;
    }

  if (!valid_fd (fd))
    return -1;

  struct file *file = thread_current ()->open_files[fd];
  if (file == NULL)
    return -1;

  lock_acquire (&filesys_lock);
  off_t bytes_written = file_write (file, buffer, (off_t) size);
  lock_release (&filesys_lock);
  return bytes_written;
}

/* Changes the next byte to be read or written in open file fd
   to position. */
void 
seek_handler (int fd, unsigned position)
{
  if (!valid_fd (fd))
    return;

  lock_acquire (&filesys_lock);
  file_seek (thread_current ()->open_files[fd], ((int32_t) position));
  lock_release (&filesys_lock);
}

/* Returns position of the next byte to be read or written 
   in open file fd. */
unsigned 
tell_handler (int fd)
{
  if (!valid_fd (fd))
    exit_handler (-1);

  lock_acquire (&filesys_lock);
  off_t pos = file_tell (thread_current ()->open_files[fd]);
  lock_release (&filesys_lock);
  return ((unsigned) pos);
}

/* Closes file descriptor fd. */
void 
close_handler (int fd)
{
  if (!valid_fd (fd))
    return;

  struct thread *cur = thread_current ();

  lock_acquire (&filesys_lock);
  /* Deny closing running executables here */

  file_close (cur->open_files[fd]);
  cur->open_files[fd] = NULL;
  lock_release (&filesys_lock);
} 

/* Checks if a pointer passed in is a null pointer, a pointer
   to unmapped virtual memory, or a pointer to kernel virtual
   address space. If so, the running process is terminated. */
void 
validate_pointer (const void *pointer)
{
 
  if (pointer == NULL || is_kernel_vaddr (pointer) || 
      pagedir_get_page (thread_current ()->pagedir, pointer) == NULL
      || (int) pointer < CODE_SEG_START)
    {
      exit_handler (-1);
    }
}

/* Checks if a buffer passed in is valid. 
   If not, the running process is terminated. */
void
validate_buffer (const void *buffer, unsigned size) 
{
  unsigned i;
  unsigned pointer_size = sizeof (const void *);
  unsigned num_pointers = size / pointer_size;

  if (size % pointer_size != 0)
    num_pointers++;

  for (i = 0; i < num_pointers; i++)
    {
      validate_pointer ((const void *) (((int *) buffer) + i));
    }
}

/* Checks if fd is in range of array. If not, return false. */
bool
valid_fd (int fd)
{
  if ((fd < FD_START) || (fd > MAX_FD_COUNT))
    return false;
  return true;
}
