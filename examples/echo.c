#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;

  printf ("Going to go ahead here and write something slightly less fucking vulgar than what was here before in order to test out how our write function handles outputting huge-ass buffers to STDOUT.\n");
  for (i = 0; i < argc; i++)
    printf ("%s ", argv[i]);
  printf ("\n");

  return EXIT_SUCCESS;
}
