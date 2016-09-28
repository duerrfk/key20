/*
 * File:    avrnacl_8bitc/crypto_hash/sha512.c
 * Author:  Michael Hutter, Peter Schwabe
 * Version: Tue Aug 12 08:23:16 2014 +0200
 * Public Domain
 */

/*
 * Based on tweetnacl.c version 20140427.
 * by Daniel J. Bernstein, Wesley Janssen, Tanja Lange, and Peter Schwabe
 */

#include "avrnacl.h"

extern const unsigned char avrnacl_sha512_iv[64];

int crypto_hash_sha512(
    unsigned char *out,
    const unsigned char *m,crypto_uint16 mlen
    )
{
  unsigned char h[64];
  unsigned char padded[256];
  crypto_uint16 i,b = mlen;

  for(i=0;i<64;i++)
    h[i] = avrnacl_sha512_iv[i];

  crypto_hashblocks_sha512(h,m,mlen);
  m += mlen;
  mlen &= 127;
  m -= mlen;

  for(i=0;i<256;i++)
    padded[i] = 0;
  for(i=0;i<mlen;i++)
    padded[i] = m[i];
  padded[mlen] = 128;

  mlen = 256-128*(mlen<112);
  
  for(i=9;i>3;i--)
    padded[mlen-i] = 0;
	padded[mlen-3] = b >> 13;
	padded[mlen-2] = b >> 5;
	padded[mlen-1] = b << 3;	

  crypto_hashblocks_sha512(h,padded,mlen);

  for(i=0;i<64;i++)
    out[i] = h[i];

  return 0;
}
