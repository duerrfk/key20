/* Linker script to configure memory regions. */

SEARCH_DIR(.)
GROUP(-lgcc -lc -lnosys)

/* nRF51822, Revision 3, Variant AA has 256 kB Flash and 16 KB of RAM. 
   With softdevice S110 version 8, 8 kB RAM (0x2000) are left.
   S110 needs no heap, and 1536 bytes stack shared with the application stack.
*/
MEMORY
{
  FLASH (rx) : ORIGIN = 0x18000, LENGTH = 0x28000
  RAM (rwx) :  ORIGIN = 0x20002000, LENGTH = 0x2000
}

SECTIONS
{
  .fs_data_out ALIGN(4):
  {
    PROVIDE( __start_fs_data = .);
    KEEP(*(fs_data))
    PROVIDE( __stop_fs_data = .);
  } = 0
}

INCLUDE "nrf5x_common.ld"
