/* 
 * syscall.h 
 *
 * Partner 1: Connie Chen, connie
 * Partner 2: Cindy Truong, cqtruong
 * Partner 3: Zachary King, zacragu
 * Date: 10/27/17
 */
#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>
 #include "threads/interrupt.h"

/* Connie driving now. */
typedef int pid_t;

/* Global lock for filesys. */
struct lock filesys_lock;

void syscall_init (void);

void get_arg (int arguments[], void *esp, int num_arg);

/* Syscall handlers. */
void halt_handler (void);
void exit_handler (int status);
pid_t exec_handler (const char *cmd_line);
int wait_handler (pid_t pid);
bool create_handler (const char *file, unsigned initial_size);
bool remove_handler (const char *file);
int open_handler (const char *file);
int filesize_handler (int fd);
int read_handler (int fd, void *buffer, unsigned size);
int write_handler (int fd, const void *buffer, unsigned size);
void seek_handler (int fd, unsigned position);
unsigned tell_handler (int fd);
void close_handler (int fd);

/* Error-checking functions. */
void validate_pointer (const void *pointer);
void validate_buffer (const void *buffer, unsigned size, struct intr_frame *f);
bool valid_fd (int fd);
/* end of Connie driving. */

#endif /* userprog/syscall.h */

