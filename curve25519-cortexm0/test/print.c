#include <stm32f0xx.h>
#include <stdio.h>

static int uart_initialized = 0;

static void initUART(){
  USART_InitTypeDef USART_InitStructure;
  GPIO_InitTypeDef GPIO_InitStructure;
 
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE);
 
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2,ENABLE);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_1);
  GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_1);
 
  //Configure USART2 pins:  Rx and Tx ----------------------------
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_2 | GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
 
  //Configure USART2 setting:         ----------------------------
  USART_InitStructure.USART_BaudRate = 9600;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART2, &USART_InitStructure);

  USART_Cmd(USART2,ENABLE);
}

void write_byte(unsigned char c)
{
  if(!uart_initialized)
  {
    initUART();
    uart_initialized = 1;
  }

  while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
  USART_SendData(USART2, (unsigned char) c);
}

void print(const char *in_s)
{
  unsigned char lp = 0;

  while(in_s[lp] != 0)
    write_byte((unsigned char) in_s[lp++]);
}

void bigint_print(const unsigned char *x, unsigned char xlen)
{
  int i;
  char ts[15];

  print("(");
  for(i=xlen-1;i>0;i--)
  {
    sprintf(ts, "%u*2^(%d*8)+",x[i],i);
    print(ts);
  }
  sprintf(ts, "%u*2^(%d*8))",x[0],i);
  print(ts);
}

void printllu(unsigned long long x)
{
  char str[24];
  int i = 22;
  str[23]=0;
  if(x==0)
    print("0");
  while(x>0)
  {
    str[i] = (char)((x%10)+48);
    i--;
    x = x/10;
  }
  print(str+i+1);
}


void print_stack(const char *primitive, const unsigned int bytes, unsigned int stack)
{
  print(primitive);
  print(": ");
  if(bytes != (unsigned int)-1)
  {
    print("[");
    printllu(bytes);
    print("] ");
  }

  printllu(stack);
    print(" stack bytes\n");
}
