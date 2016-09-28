/*
 * File:    avrnacl_8bitc/crypto_verify/verify.c
 * Author:  Michael Hutter, Peter Schwabe
 * Version: Tue Aug 12 08:23:16 2014 +0200
 * Public Domain
 */

/*
 * Based on tweetnacl.c version 20140427.
 * by Daniel J. Bernstein, Wesley Janssen, Tanja Lange, and Peter Schwabe
 */

#include "avrnacl.h"

static int vn(const unsigned char *x,const unsigned char *y,char n)
{
  crypto_uint8 i;
  crypto_uint16 d = 0;
  for(i=0;i<n;i++) 
    d |= x[i]^y[i];
  return (1 & ((d - 1) >> 8)) - 1;
}

int crypto_verify_16(
    const unsigned char *x,
    const unsigned char *y
    )
{
  return vn(x,y,16);
}

int crypto_verify_32(
    const unsigned char *x,
    const unsigned char *y
    )
{
  return vn(x,y,32);
}
