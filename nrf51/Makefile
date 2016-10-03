NRF51_SDK = /home/duerrfk/local/nRF51_SDK_10.0.0
CURVE25519 = ../curve25519-cortexm0
AVRNACL = ../avrnacl
HD44780NRF51 = ../hd44780nrf51

CROSS = /usr/local/gcc-arm-none-eabi-5_2-2015q4/bin/arm-none-eabi-

SRC += key20.c 
SRC += app_event_queue.c
SRC += $(NRF51_SDK)/components/toolchain/system_nrf51.c 
SRC += $(NRF51_SDK)/components/drivers_nrf/delay/nrf_delay.c
SRC += $(NRF51_SDK)/components/softdevice/common/softdevice_handler/softdevice_handler.c
SRC += $(NRF51_SDK)/components/libraries/util/app_error.c
SRC += $(NRF51_SDK)/components/ble/common/ble_advdata.c
SRC += $(NRF51_SDK)/components/libraries/button/app_button.c
SRC += $(NRF51_SDK)/components/libraries/timer/app_timer.c
SRC += $(NRF51_SDK)/components/drivers_nrf/gpiote/nrf_drv_gpiote.c
SRC += $(NRF51_SDK)/components/drivers_nrf/common/nrf_drv_common.c
SRC += $(NRF51_SDK)/components/drivers_nrf/pstorage/pstorage.c
SRC += $(CURVE25519)/scalarmult.c
SRC += $(AVRNACL)/crypto_hash/sha512.c
SRC += $(AVRNACL)/crypto_hashblocks/sha512.c
SRC += $(AVRNACL)/crypto_auth/hmac.c
SRC += $(AVRNACL)/crypto_verify/verify.c
SRC += $(AVRNACL)/shared/consts.c
SRC += $(AVRNACL)/shared/bigint.c
SRC += $(HD44780NRF51)/hd44780nrf51.c

#ASM_SRC = $(NRF51_SDK)/components/toolchain/gcc/gcc_startup_nrf51.s
ASM_SRC = gcc_startup_nrf51.s
ASM_SRC += $(CURVE25519)/cortex_m0_mpy121666.s
ASM_SRC += $(CURVE25519)/cortex_m0_reduce25519.s
ASM_SRC += $(CURVE25519)/mul.s
ASM_SRC += $(CURVE25519)/sqr.s

OUTPUT = key20

TEMPLATE_PATH = $(NRF51_SDK)/components/toolchain/gcc

INCLUDES += -I. 
INCLUDES += -I$(NRF51_SDK)/components/device 
INCLUDES += -I$(NRF51_SDK)/components/drivers_nrf/delay 
INCLUDES += -I$(NRF51_SDK)/components/toolchain 
INCLUDES += -I$(NRF51_SDK)/components/toolchain/gcc 
INCLUDES += -I$(NRF51_SDK)/components/drivers_nrf/hal
INCLUDES += -I$(NRF51_SDK)/components/softdevice/s110/headers
INCLUDES += -I$(NRF51_SDK)/components/softdevice/common/softdevice_handler
INCLUDES += -I$(NRF51_SDK)/components/libraries/util
INCLUDES += -I$(NRF51_SDK)/components/ble/common
INCLUDES += -I$(NRF51_SDK)/components/libraries/timer
INCLUDES += -I$(NRF51_SDK)/components/libraries/button
INCLUDES += -I$(NRF51_SDK)/components/drivers_nrf/gpiote
INCLUDES += -I$(NRF51_SDK)/components/drivers_nrf/config
INCLUDES += -I$(NRF51_SDK)/components/drivers_nrf/common
INCLUDES += -I$(NRF51_SDK)/components/drivers_nrf/pstorage
INCLUDES += -I$(CURVE25519)
INCLUDES += -I$(AVRNACL)
INCLUDES += -I$(AVRNACL)/include
INCLUDES += -I$(HD44780NRF51)

C_OBJ = $(SRC:.c=.o)
ASM_OBJ = $(ASM_SRC:.s=.o)

OPTIMIZATION = -O0

CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

# For nRF51 DK, select nrf51422_ac_s100.ld.
# For productive version using nRF51822, select nrf51822_aa_s110.ld.
#LINKER_SCRIPT = nrf51422_ac_s110.ld
LINKER_SCRIPT = nrf51822_aa_s110.ld

CFLAGS += -mcpu=cortex-m0 -mthumb -mabi=aapcs -mfloat-abi=soft
CFLAGS += --std=gnu99
# The following options enable the linker to remove unused functions.
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fno-strict-aliasing
CFLAGS += -Wall
CFLAGS += -fno-builtin --short-enums
CFLAGS += $(OPTIMIZATION) $(INCLUDES)
CFLAGS += -DNRF51
CFLAGS += -DBLE_STACK_SUPPORT_REQD

ASMFLAGS += -x assembler-with-cpp -mcpu=cortex-m0 -mthumb -mabi=aapcs -mfloat-abi=soft

LDFLAGS += -Xlinker -Map=$(OUTPUT).map
LDFLAGS += -mcpu=cortex-m0 -mthumb -mabi=aapcs 
LDFLAGS += -L $(TEMPLATE_PATH) -T$(LINKER_SCRIPT)
# Let linker remove unused sections
LDFLAGS += -Wl,--gc-sections
# Use newlib in nano version
LDFLAGS += --specs=nano.specs 
LDFLAGS += -lc -lnosys

all: $(OUTPUT).hex

# Build objects from C source code
$(C_OBJ): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build objects from assembler code
$(ASM_OBJ): %.o: %.s
	$(CC) $(ASMFLAGS) -c $< -o $@

# Link
$(OUTPUT).out: $(C_OBJ) $(ASM_OBJ)
	$(CC) $(LDFLAGS) $(C_OBJ) $(ASM_OBJ) -o $@

# Create binary .hex file from the .out file
$(OUTPUT).hex: $(OUTPUT).out
	$(OBJCOPY) -O ihex $< $@

.PHONY: clean
clean:
	rm $(OUTPUT).hex $(OUTPUT).out $(ASM_OBJ) $(C_OBJ)
