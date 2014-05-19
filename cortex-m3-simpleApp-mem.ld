/* 
 * Linker script.
 * Este archivo describe la distrubución de memoria en el nodo
 * hijo en el modo aplicacion. La diferencia con uno por defecto
 * (como es el del maestro) esta en la longitud de la seccion de 
 * memoria marcada como FLASH y en la posicion de inicio de la misma.
 *
 * En el caso de la aplicacion la longitud de la secion de FLASH
 * de la memoria es el total menos el tamano del la seccion FLASH
 * del bootloader (definido en mem_m2c_bootloader). Además, el inicio
 * de la seccion FLASH en el modo de aplicacion se establece estrictamente
 * al final de la seccion del bootloader. 
 *
 * EJEMPLO:
 * Tenemos un bootloader de tamano BL_L, una tamano de FLASH total de
 * TL_L (especificado por el fabricante en funcion del modelo de procesador)
 * y la direccion de inicio absoluta de FLASH es la 0x08000000 (comun a 
 * todos los procesadores de STM.
 *
 * Tendremos la distribucion de memoria como sigue:
 * En el archivo correspondiente al bootloader:
 * 		FLASH (rx): ORIGIN = 0x08000000       , LENGTH = BL_L
 * En el archivo correspondiente a la aplicación:
 * 		FLASH (rx): ORIGIN = 0x08000000 + BL_L, LENGTH = TL_L - BL_L
 *
 * Además es importante que una vez este definido el tamano del bootloader
 * (que tambien determina el origen y el tamano de las otras secciones)
 * se defina en el archivo m2c_defs.h lo siguiente:
 * 		#define APPLICATION_OFFSET  ((uint32_t)0x5000)
 */
MEMORY
{
  FLASH (rx)      : ORIGIN = 0x08005000, LENGTH = 64K-0x5000 /* ~45K */
  NVM_region (rx) : ORIGIN = 0x0801F800, LENGTH = 2K
  RAM (rwx)       : ORIGIN = 0x20000000, LENGTH = 8K
  FIB_region (ra) :	ORIGIN = 0x08040000, LENGTH = 2K
}