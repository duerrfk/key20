#include <errno.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "stm32f0xx_usart.h"

#undef errno
extern int errno;

#ifndef __caddr_t_defined
typedef __caddr_t caddr_t;
#endif

caddr_t _sbrk(int incr) 
{

  extern char _ebss; // Defined by the linker
  static char *heap_end;
  char *prev_heap_end;

  if (heap_end == 0) {
    heap_end = &_ebss;
  }
  prev_heap_end = heap_end;

  char * stack = (char*) __get_MSP();
  if (heap_end + incr >  stack)
  {
    //_write (STDERR_FILENO, "Heap and stack collision\n", 25);
    errno = ENOMEM;
    return  (caddr_t) -1;
    // abort ();
  }

  heap_end += incr;
  return (caddr_t) prev_heap_end;

}
