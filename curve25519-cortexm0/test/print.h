#ifndef PRINT_H
#define PRINT_H

void write_byte(unsigned char c);

void print(const char *s);

void bigint_print(const unsigned char *x, unsigned char xlen);

void print_stack(const char *primitive, const unsigned int bytes, unsigned int stack);

#endif
