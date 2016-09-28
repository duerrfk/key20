CC= clang
CFLAGS= -fshort-enums -O3 -DCORTEX_M0 -mthumb -ffunction-sections -fdata-sections -fmessage-length=0 -mcpu=cortex-m0 -fno-builtin  -ffreestanding -target arm-none-eabi -mfloat-abi=soft -nostdlib -no-integrated-as
ASFLAGS= -mcpu=cortex-m0 -target arm-none-eabi -mfloat-abi=soft -nostdlib -no-integrated-as
INCDIRS=-I./stm32f0xx
AR = arm-none-eabi-ar

STMOBJ = stm32f0xx/system_stm32f0xx.o  \
         stm32f0xx/stm32f0xx_gpio.o  \
         stm32f0xx/stm32f0xx_rcc.o  \
         stm32f0xx/stm32f0xx_usart.o  \
         stm32f0xx/startup_stm32f0xx.o \
				 stm32f0xx/sbrk.o

LINKERFILE = stm32f0xx/stm32f0_linker.ld

all: test/speed.bin test/test.bin test/stack.bin

test/speed.elf: $(STMOBJ) test/speed.c test/print.c obj/curve25519.a 
	$(CC) $(CFLAGS) $(INCDIRS) -T $(LINKERFILE) $(STMOBJ)  test/speed.c test/print.c obj/curve25519.a -o $@

test/stack.elf: $(STMOBJ) test/stack.c test/print.c obj/curve25519.a 
	$(CC) $(CFLAGS) $(INCDIRS) -T $(LINKERFILE) $(STMOBJ)  test/stack.c test/print.c test/randombytes.c obj/curve25519.a -o $@

test/test.elf: $(STMOBJ) test/test.c obj/curve25519.a 
	$(CC) $(CFLAGS) $(INCDIRS) -T $(LINKERFILE) $(STMOBJ) test/test.c test/print.c test/randombytes.c test/fail.c obj/curve25519.a -o $@


obj/curve25519.a: obj/scalarmult.o \
							obj/cortex_m0_mpy121666.o  \
							obj/cortex_m0_reduce25519.o  \
							obj/sqr.o \
							obj/mul.o
	mkdir -p obj/
	$(AR) -ar cr $@ $^ 


obj/scalarmult.o: scalarmult.s
	mkdir -p obj/
	$(CC) $(ASFLAGS) -c $^ -o $@

scalarmult.s: scalarmult_opt.bc
	llc -misched=ilpmin -enable-misched -misched-regpressure scalarmult_opt.bc -o scalarmult.s 

scalarmult_opt.bc: scalarmult.c
	clang -fshort-enums -mthumb -mcpu=cortex-m0 -emit-llvm -c -nostdlib -ffreestanding -target arm-none-eabi  -mfloat-abi=soft scalarmult.c -I /usr/arm-linux-gnueabi/include 
	opt -Os -inline -misched=ilpmin -enable-misched -misched-regpressure scalarmult.bc -o scalarmult_opt.bc

%.bin: %.elf
		 arm-none-eabi-objcopy  -O binary $^ $@

obj/%.o: %.c
	mkdir -p obj/
	$(CC) $(CFLAGS) $(INCDIRS) -c $^ -o $@

obj/%.o: %.s
	mkdir -p obj/
	$(CC) $(ASFLAGS) -c $^ -o $@

stm32f0xx/%.o: stm32f0xx/%.c
	$(CC) $(CFLAGS) $(INCDIRS) -c $^ -o $@

stm32f0xx/%.o: stm32f0xx/%.s
	$(CC) $(ASFLAGS) -c $^ -o $@

.PHONY: clean

clean:
	-rm obj/*
	-rm scalarmult.s
	-rm scalarmult_opt.bc
	-rm stm32f0xx/*.o
	-rm test/test.elf
	-rm test/test.bin
	-rm test/speed.elf
	-rm test/stack.bin
	-rm test/stack.elf
	-rm test/speed.bin
