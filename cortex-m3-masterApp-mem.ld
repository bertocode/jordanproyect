/* 
 * Linker script.
 * Este archivo describe la distrubución de memoria en el nodo
 * maestro. En este caso es el mismo que por defecto nos proporciona
 * STM32.
 */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 64K
  NVM_region (rx) : ORIGIN = 0x0801F800, LENGTH = 2K
  RAM (rwx)       : ORIGIN = 0x20000000, LENGTH = 8K
  FIB_region (ra) : ORIGIN = 0x08040000, LENGTH = 2K
}