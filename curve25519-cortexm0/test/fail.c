#include <stdlib.h>
#include "fail.h"
#include "print.h"

void fail(const char *error)
{
  print("ERROR: ");
  print(error);
  print("\n");
  //exit(-1);
}
