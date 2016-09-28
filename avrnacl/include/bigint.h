#ifndef BIGINT_H
#define BIGINT_H

/* Arithmetic on big integers represented as arrays of unsigned char in radix 2^256 */

#define bigint_add avrnacl_bigint_add
#define bigint_sub avrnacl_bigint_sub
#define bigint_mul avrnacl_bigint_mul
#define bigint_mul32 avrnacl_bigint_mul32
#define bigint_cmov avrnacl_bigint_cmov

unsigned char bigint_add(unsigned char r[32], const unsigned char a[32], const unsigned char b[32], unsigned int len);

unsigned char bigint_sub(unsigned char r[32], const unsigned char a[32], const unsigned char b[32], unsigned int len);

void bigint_mul(unsigned char *r, const unsigned char *a, const unsigned char *b, unsigned int len);

void bigint_mul32(unsigned char *r, const unsigned char *a, const unsigned char *b);

void bigint_cmov(unsigned char *r, const unsigned char *x, unsigned char b, unsigned int len);

#endif
