#include <stdio.h>
#include <stm32f0xx.h>
#include "print.h"
#include "../api.h"

int main(void)
{
  char out[500];

  unsigned char secretKeyAlice[32];/* =
                                      { 0x77, 0x07, 0x6d, 0x0a, 0x73, 0x18, 0xa5, 0x7d, 0x3c, 0x16,
                                      0xc1, 0x72, 0x51, 0xb2, 0x66, 0x45, 0xdf, 0x4c, 0x2f,
                                      0x87, 0xeb, 0xc0, 0x99, 0x2a, 0xb1, 0x77, 0xfb, 0xa5,
                                      0x1d, 0xb9, 0x2c, 0x2a };*/

  unsigned char expectedPublicKeyBob[32]; /*=
                                            { 0xde, 0x9e, 0xdb, 0x7d, 0x7b, 0x7d, 0xc1, 0xb4, 0xd3, 0x5b,
                                            0x61, 0xc2, 0xec, 0xe4, 0x35, 0x37, 0x3f, 0x83, 0x43,
                                            0xc8, 0x5b, 0x78, 0x67, 0x4d, 0xad, 0xfc, 0x7e, 0x14,
                                            0x6f, 0x88, 0x2b, 0x4f };*/

  unsigned char sharedSecretCalculatedByAlice[32];
  SysTick_Config(12000000);
  SysTick->VAL = 0;
  int ini, fin;

  ini= SysTick->VAL;
  crypto_scalarmult_curve25519(sharedSecretCalculatedByAlice, secretKeyAlice, expectedPublicKeyBob);
  fin= SysTick->VAL;

  fin = ini-fin;

  sprintf(out, "Cycles: %d.", fin);
  print(out);
  print("\n");
  
  write_byte(4);

  while(1);
}

void SysTick_Handler(void)
{
  ;
}
