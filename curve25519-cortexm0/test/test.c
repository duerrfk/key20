/*
 * Based on crypto_scalarmult/try.c version 20090118 by D. J. Bernstein
 * Public domain.
 */

#include <stdlib.h>
#include "../api.h"
#include "print.h"
#include "fail.h"
#include "randombytes.h"


#define mlen crypto_scalarmult_SCALARBYTES
#define nlen crypto_scalarmult_SCALARBYTES
#define plen crypto_scalarmult_BYTES
#define qlen crypto_scalarmult_BYTES
#define rlen crypto_scalarmult_BYTES


static unsigned char gm[mlen+32];
static unsigned char gn[nlen+32];
static unsigned char gp[plen+32];
static unsigned char gq[qlen+32];
static unsigned char gr[rlen+32];

static unsigned char gm2[mlen+32];
static unsigned char gn2[nlen+32];
static unsigned char gp2[plen+32];
static unsigned char gq2[qlen+32];
static unsigned char gr2[rlen+32];

unsigned char *m = gm;
unsigned char *n = gn;
unsigned char *p = gp;
unsigned char *q = gq;
unsigned char *r = gr;

unsigned char *m2 = gm2;
unsigned char *n2 = gn2;
unsigned char *p2 = gp2;
unsigned char *q2 = gq2;
unsigned char *r2 = gr2;


static char checksum[crypto_scalarmult_BYTES * 2 + 1];

static const unsigned char badn[32] = {0x56,0x2c,0x1e,0xb5,0xfd,0xb2,0x81,0x29,
                                       0xbd,0x37,0x49,0x58,0x35,0xd4,0xb1,0x30,
                                       0x7d,0xdb,0x57,0x38,0x80,0x12,0x17,0x42,
                                       0xf7,0x13,0xf1,0x05,0x67,0x69,0xd5,0xbf};



int main(void)
{
  long long i;
  long long j;
  long long tests;

  m += 16;
  n += 16;
  p += 16;
  q += 16;
  r += 16;
  m2 += 16;
  n2 += 16;
  p2 += 16;
  q2 += 16;
  r2 += 16;

  for (i = 0;i < mlen;++i) m[i] = i;
  for (i = 0;i < nlen;++i) n[i] = i + 1;
  for (i = 0;i < plen;++i) p[i] = i + 2;
  for (i = 0;i < qlen;++i) q[i] = i + 3;
  for (i = 0;i < rlen;++i) r[i] = i + 4;

  randombytes(p-16,16);
  randombytes(n-16,16);
  randombytes(p+plen,16);
  randombytes(n+nlen,16);
  for (i = -16;i < plen + 16;++i) p2[i] = p[i];
  for (i = -16;i < nlen + 16;++i) n2[i] = n[i];
  
  if (crypto_scalarmult_base(p,n) != 0) fail("crypto_scalarmult_base returns nonzero");

  for (i = -16;i < nlen + 16;++i) if (n2[i] != n[i]) fail("crypto_scalarmult_base overwrites input");
  for (i = -16;i < 0;++i) if (p2[i] != p[i]) fail("crypto_scalarmult_base writes before output");
  for (i = plen;i < plen + 16;++i) if (p2[i] != p[i]) fail("crypto_scalarmult_base writes after output");

  crypto_scalarmult_base(p,badn);

  for (tests = 0;tests < 5;++tests) 
  {
    randombytes(p-16,16);
    randombytes(q-16,16);
    randombytes(m-16,16);
    randombytes(p+plen,16);
    randombytes(q+qlen,16);
    randombytes(m+mlen,16);
    for (i = -16;i < qlen + 16;++i) q2[i] = q[i];
    for (i = -16;i < plen + 16;++i) p2[i] = p[i];
    for (i = -16;i < mlen + 16;++i) m2[i] = m[i];

    if (crypto_scalarmult(q,m,p) != 0) fail("crypto_scalarmult returns nonzero");

    for (i = -16;i < mlen + 16;++i) if (m2[i] != m[i]) fail("crypto_scalarmult overwrites n input");
    for (i = -16;i < plen + 16;++i) if (p2[i] != p[i]) fail("crypto_scalarmult overwrites p input");
    for (i = -16;i < 0;++i) if (q2[i] != q[i]) fail("crypto_scalarmult writes before output");
    for (i = qlen;i < qlen + 16;++i) if (q2[i] != q[i]) fail("crypto_scalarmult writes after output");

    if (crypto_scalarmult(m2,m2,p) != 0) fail("crypto_scalarmult returns nonzero");
    for (i = 0;i < qlen;++i) if (q[i] != m2[i]) fail("crypto_scalarmult does not handle n overlap");
    for (i = 0;i < qlen;++i) m2[i] = m[i];

    if (crypto_scalarmult(p2,m2,p2) != 0) fail("crypto_scalarmult returns nonzero");
    for (i = 0;i < qlen;++i) if (q[i] != p2[i]) fail("crypto_scalarmult does not handle p overlap");

    if (crypto_scalarmult(r,n,q) != 0) fail("crypto_scalarmult returns nonzero");
    if (crypto_scalarmult(q,n,p) != 0) fail("crypto_scalarmult returns nonzero");
    if (crypto_scalarmult(p,m,q) != 0) fail("crypto_scalarmult returns nonzero");
    for (j = 0;j < plen;++j) if (p[j] != r[j]) fail("crypto_scalarmult not associative");
    for (j = 0;j < mlen;++j) m[j] ^= q[j % qlen];
    for (j = 0;j < nlen;++j) n[j] ^= p[j % plen];
  }

  for (i = 0;i < crypto_scalarmult_BYTES;++i) {
    checksum[2 * i] = "0123456789abcdef"[15 & (p[i] >> 4)];
    checksum[2 * i + 1] = "0123456789abcdef"[15 & p[i]];
  }
  checksum[2 * i] = 0;

  print(checksum);
  print("\n");

  write_byte(4);

  while(1);
}
