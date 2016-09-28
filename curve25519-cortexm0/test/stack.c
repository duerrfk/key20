#include <stdlib.h>
#include "../api.h"
#include "print.h"
#include "fail.h"
#include "randombytes.h"

#define nlen crypto_scalarmult_SCALARBYTES
#define qlen crypto_scalarmult_BYTES

#define MAXSTACK 1000

unsigned char i;
unsigned char n[nlen];
unsigned char q[qlen];

unsigned int ctr;
unsigned char canary;
volatile unsigned char *p;
extern unsigned char _end; 

static unsigned int stack_count(unsigned char canary,volatile unsigned char *a)
{
  volatile unsigned char *p = (a-MAXSTACK);
  unsigned int c = 0;
  while(*p == canary && p < a)
  {
    p++;
    c++;
  }
  return c;
} 

#define WRITE_CANARY(X) {p=X;while(p>= (X-MAXSTACK)) *(p--) = canary;}
 
int main(void)
{
  volatile unsigned char a; /* Mark the beginning of the stack */

  //randombytes(&canary,1);
  canary = 42;

  WRITE_CANARY(&a);
  crypto_scalarmult(q,n,q);
  ctr = MAXSTACK - stack_count(canary,&a);
  print_stack("crypto_scalarmult",-1,ctr);

  write_byte(4);
  while(1);
}
