#include "userprog/syscall.h"
#include <stdio.h>
#include <stdlib.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "pagedir.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "filesys/off_t.h"

#define MAX_ARGS 3
#define BLOCK_SIZE 200
#define FD_START 2
#define MAX_FD_COUNT 256

/* Global lock for filesys. */
static struct lock filesys_lock;

/* Index through fd to get pointer to associated file struct. */         
struct file *open_files[MAX_FD_COUNT];

/* Index through fd to get associated tid. */
tid_t file_associated_tids[MAX_FD_COUNT];
int fd_count = FD_START;

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  int i;
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
  for (i = 0; i < MAX_FD_COUNT; i++)
    file_associated_tids[i] = -1;
}

static void
syscall_handler (struct intr_frame *f) 
{
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
     arguments[i] = *local_esp;
   }

   /* Write debugging printf statements */
  /* if (num_arg == 3) 
   {
     printf("fd: %d\n", (int) arguments[0]);
     printf("buffer: %p\n", arguments[1]);
     printf("size: %u\n", (unsigned) arguments[2]);
   }*/
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
  //printf("Here exit_handler !\n");
  thread_current ()->parent->child_exit_status = status;
  /* printf("child exit status: %d\n", thread_current ()->parent->
          child_exit_status); */
  printf ("%s: exit(%d)\n", thread_current ()->name, status);
  process_exit ();
  thread_exit ();
}

/* Runs the executable whose name is given in cmd_line.
   Returns the program's PID. If the program cannot 
   load or run for any reason, returns -1. */
pid_t 
exec_handler (const char *cmd_line)
{
  lock_acquire (&filesys_lock);
  lock_release (&filesys_lock);
  return -1;
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
  return -1;
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
  lock_acquire (&filesys_lock);
  int fd_index;
  for (fd_index = FD_START; fd_index < MAX_FD_COUNT; fd_index++)
    {
      if (file_associated_tids[fd_index] == -1)
        {
          open_files[fd_index] = filesys_open (file);
          if (open_files[fd_index] == NULL)
            break;
          file_associated_tids[fd_index] = thread_current ()->tid;
          lock_release (&filesys_lock);
          return fd_index;
        }
    }
  lock_release (&filesys_lock);
  return -1;
}

/* Returns the size, in bytes, of the file open as fd. */
int 
filesize_handler (int fd) 
{
  ASSERT (valid_fd (fd));

  lock_acquire (&filesys_lock);
  int size = file_length (open_files[fd]);
  lock_release (&filesys_lock);
  return size;
}

/* Reads size bytes from the file open as fd into buffer. 
   Returns the number of bytes actually read (0 at end of file),
   or -1 if the file could not be read. */
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

  ASSERT (valid_fd (fd));
  if ((open_files[fd] == NULL))
    return -1;

  lock_acquire (&filesys_lock);
  size_read = file_read (open_files[fd], buffer, (off_t) size);
  lock_release (&filesys_lock);
  return size_read;
}

/* Write size bytes from buffer to the open file fd. Returns
   the number of bytes actually written. 

   fd 1 writes to the console. */
int 
write_handler (int fd, const void *buffer, unsigned size)
{
  //printf("Here write_handler !\n");
  if (fd == STDOUT_FILENO) /* Is there already a macro defined for STDOUT? */
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
      //printf("write_handler returned size: %u\n", size);
      return size;
    }

  ASSERT (valid_fd (fd));
  if ((open_files[fd] == NULL))
    return -1;

  lock_acquire (&filesys_lock);
  struct file *target = open_files[fd];
  off_t bytes_written = file_write (target, buffer, (off_t) size);
  lock_release (&filesys_lock);
  return bytes_written;
}

/* Changes the next byte to be read or written in open file fd
   to position. */
void 
seek_handler (int fd, unsigned position)
{
  ASSERT (valid_fd (fd));

  lock_acquire (&filesys_lock);
  file_seek (open_files[fd], ((int32_t) position));
  lock_release (&filesys_lock);
}

/* Returns position of the next byte to be read or written 
   in open file fd. */
unsigned 
tell_handler (int fd)
{
  ASSERT (valid_fd (fd));

  lock_acquire (&filesys_lock);
  off_t pos = file_tell (open_files[fd]);
  lock_release (&filesys_lock);
  return ((unsigned) pos);
}

/* Closes file descriptor fd. */
void 
close_handler (int fd)
{
  ASSERT (valid_fd (fd));

  lock_acquire (&filesys_lock);
  lock_release (&filesys_lock);
} 

/* Checks if a pointer passed in is a null pointer, a pointer
   to unmapped virtual memory, or a pointer to kernel virtual
   address space. If so, the running process is terminated. */
void 
validate_pointer (const void *pointer)
{
  if (pointer == NULL || is_kernel_vaddr (pointer) || 
      pagedir_get_page (thread_current ()->pagedir, pointer) == NULL)
    {
      /* call thread_exit() or call exit()?  */
      thread_exit ();
    }
}

/* Checks if a buffer passed in is valid. 
   If not, the running process is terminated. */
void
validate_buffer (const void *buffer, unsigned size) 
{
  unsigned i;
  unsigned pointer_size = sizeof(const void *);

  for (i = 0; i < ((size / pointer_size) + (size % pointer_size)); i++)
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
