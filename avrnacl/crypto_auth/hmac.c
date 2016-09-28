/*
 * File:    avrnacl_8bitc/crypto_auth/hmac.c
 * Author:  Michael Hutter, Peter Schwabe
 * Version: Tue Aug 12 08:23:16 2014 +0200
 * Public Domain
 */


/*
 * Based on crypto_auth/hmacsha512256/ref, version 20080913
 * by D. J. Bernstein (Public domain).
 * */

#include "avrnacl.h"

#define blocks crypto_hashblocks_sha512

typedef unsigned long long uint64;

extern const unsigned char avrnacl_sha512_iv[64];

int crypto_auth_hmacsha512256(
    unsigned char *out,
    const unsigned char *in, crypto_uint16 inlen,
    const unsigned char *k
    )
{
  unsigned char h[64];
  unsigned char padded[256];
  unsigned int i;
  unsigned int bytes = 128 + inlen;

  for (i = 0;i < 64;++i) h[i] = avrnacl_sha512_iv[i];

  for (i = 0;i < 32;++i) padded[i] = k[i] ^ 0x36;
  for (i = 32;i < 128;++i) padded[i] = 0x36;

  blocks(h,padded,128);
  blocks(h,in,inlen);
  in += inlen;
  inlen &= 127;
  in -= inlen;

  for (i = 0;i < inlen;++i) padded[i] = in[i];
  padded[inlen] = 0x80;

  if (inlen < 112) {
    for (i = inlen + 1;i < 125;++i) padded[i] = 0;
    padded[125] = bytes >> 13;
    padded[126] = bytes >> 5;
    padded[127] = bytes << 3;
    blocks(h,padded,128);
  } else {
    for (i = inlen + 1;i < 253;++i) padded[i] = 0;
    padded[253] = bytes >> 13;
    padded[254] = bytes >> 5;
    padded[255] = bytes << 3;
    blocks(h,padded,256);
  }

  for (i = 0;i < 32;++i) padded[i] = k[i] ^ 0x5c;
  for (i = 32;i < 128;++i) padded[i] = 0x5c;

  for (i = 0;i < 64;++i) padded[128 + i] = h[i];
  for (i = 0;i < 64;++i) h[i] = avrnacl_sha512_iv[i];

  for (i = 64;i < 128;++i) padded[128 + i] = 0;
  padded[128 + 64] = 0x80;
  padded[128 + 126] = 6;

  blocks(h,padded,256);
  for (i = 0;i < 32;++i) out[i] = h[i];

  return 0;
}


int crypto_auth_hmacsha512256_verify(
    const unsigned char *h,
    const unsigned char *in,crypto_uint16 inlen,
    const unsigned char *k
    )
{
  unsigned char correct[32];
  crypto_auth_hmacsha512256(correct,in,inlen,k);
  return crypto_verify_32(h,correct);
}
