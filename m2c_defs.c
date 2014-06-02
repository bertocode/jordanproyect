#include "m2c_defs.h"

/**
 * Cambia y almacena el flash el nuevo valor del bootMode. Este valor determina en que modo arrancará el procesador.
 * Al cambiar este valor toda la pagina asociada a la direccion de memoria CONFIG_MASK_ADDRESS se pierde y solo se
 * mantiene el valor estrictamente en CONFIG_MASK_ADDRESS, que es precisamente la mascara de configuracion para arranque.
 */
void M2C_setBootMode(uint8_t bootMode)
{
	uint32_t newConfigValue = bootMode << 24 | USER_NODE_VERSION;

	FPEC_ClockCmd(ENABLE);

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP| FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	FLASH_ErasePage(CONFIG_MASK_ADDRESS);
	FLASH_ProgramWord(CONFIG_MASK_ADDRESS, newConfigValue);

	FLASH_Lock();
	FPEC_ClockCmd(DISABLE);
}

/**
 * Cambia y almacena en flash el nuevo valor de version del nodo.
 * Al cambiar este valor toda la pagina asociada a la direccion de memoria CONFIG_MASK_ADDRESS se pierde y solo se
 * mantiene el valor estrictamente en CONFIG_MASK_ADDRESS, que es precisamente la mascara de configuracion para arranque.
 */
void M2C_setNodeVersion(uint16_t* version)
{
	uint32_t newConfigValue = BOOT_MODE << 24 | *version;

	FPEC_ClockCmd(ENABLE);

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP| FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	FLASH_ErasePage(CONFIG_MASK_ADDRESS);
	FLASH_ProgramWord(CONFIG_MASK_ADDRESS, newConfigValue);
	FLASH_Lock();

	FPEC_ClockCmd(DISABLE);
}

/**
 * Salto de ejecución a la seccion de memoria donde deberia estar grabado
 * el binario de la aplicación.
 *
 * En esta seccion la estructura de los primeros bytes es la siguiente.
 * Asumiendo que APPLICATION_ADDRESS es estrictamente la primera posicion
 * del la seccion de memoria antes mencionada:
 *
 * APPLICATION_ADDRESS contiene el valor de inicialización del MSP
 * APPLICATION_ADDRESS + 4 contiene la direccion de memoria en la que se
 * encuentra el metodo main
 */
void M2C_jumpToApplication()
{
	uint32_t* msp_address = (uint32_t*) APPLICATION_ADDRESS;
	uint32_t* app_address = (uint32_t*)(APPLICATION_ADDRESS + 4);
	pFunction Start = (pFunction)(*app_address);
	__set_MSP(*msp_address);
    NVIC_SetVectorTable(NVIC_VectTab_FLASH, APPLICATION_OFFSET);
	Start();
}

/**
 * Salto de ejecución al bootloader.
 * Realmente solo generamos un reset.
 */
void M2C_jumpToBootloader()
{
	NVIC_SystemReset();
}

// Rutina de inicializacion de la radio
void M2C_radioInit(RadioPacket* rPacket)
{
    uint32_t seed;

    ST_RadioSetNodeId (USER_NODE_ID);
    ST_RadioSetPanId (USER_PAN_ID);
    /* Enable coordinator feature (default is disabled)*/
    ST_RadioSetCoordinator(FALSE);

    /* Initialize random number generation. */
    ST_RadioGetRandomNumbers((uint16_t *)&seed, 2);
    halCommonSeedRandom(seed);

    /* Initialize the radio. */
    ST_RadioEnableOverflowNotification(TRUE);
    assert (ST_RadioInit(ST_RADIO_POWER_MODE_RX_ON) == ST_SUCCESS);

    /* Set promiscuous mode: receive any packet on the selected radio channel */
    ST_RadioEnableAddressFiltering(FALSE);
    ST_RadioEnableAutoAck(TRUE);
    ST_RadioEnableReceiveCrc (TRUE);

    ST_RadioWake();
    ST_RadioSetPower (M2C_RADIO_POWER);
    ST_RadioSetChannel(M2C_RADIO_CHANNEL);

    // packet length including two bytes for the CRC
    rPacket->fcf1 = 0x61; // FCF - data frame type, no security, no frame pending, ack request, intra PAN
    rPacket->fcf2 = 0x88; // FCF - 16 bits short destination address, no source pan id and address
    rPacket->seq =  0x00; // sequence number
    rPacket->dest_pan_id     = (uint16_t) USER_PAN_ID; // destination pan id
    rPacket->dest_short_addr = (uint16_t) USER_GW_ID; // destination short address/node id
    rPacket->src_short_addr  = (uint16_t) USER_NODE_ID; // source short address/node id
}

/**
 * Envia el paquete pasado como parametro.
 * Si se especifican valores de waitingForTxRadioStatus y txStatus la llamada a este metodo
 * bloqueará la ejecución hasta asegurarse de que ha recivido el ACK correspondiente al envio del paquete,
 * reenviando el paquete cada vez que se reciba una respuesta no satisfactoria hasta agotar el numero
 * de reintentos especificado en el parametro retryCount.
 *
 * @params:
 * - rPacket: Puntero al paquete que quiere ser transmitido
 * - waitingForTxRadioStatus: Puntero a la variable que determina si tenemos el resultado de la emision.
 * - StStatus: Puntero al estado de la emision.
 * - retryCounter: numero de reintentos antes de desistir en el envio.
 *
 * @returns:
 *  true si se ha ejecutado en modo de bloqueo y se ha conseguido enviar el paquete sin agotar el numero de reintentos.
 *  false si se ha ejecutado sin bloqueo o bien si se han agotado los reintentos al enviar el paquete.
 */
boolean M2C_sendPacket_Locking(RadioPacket* rPacket, __IO boolean* _waitingForTxRadioStatus, __IO StStatus* _txStatus, int16_t retryCounter)
{
	boolean lockExecution = _waitingForTxRadioStatus && _txStatus;

    // Enviamos el paquete
    do
    {
    	StStatus radioStatus;

    	// Enviamos el paquete hasta que tenemos una respuesta satisfactoria
    	// Esto es, cuando sabemos que el paquete esta listo para transmitir
    	do
    	{
    		if (_txStatus)
    			*_txStatus = 0;
    		if (_waitingForTxRadioStatus)
    			*_waitingForTxRadioStatus = TRUE;

			radioStatus = ST_RadioTransmit ( (uint8_t*) rPacket);
			WDG_ReloadCounter();
    	}
    	while(radioStatus != ST_SUCCESS);

        // Esperamos hasta terminar de transmitir
        while (lockExecution && *_waitingForTxRadioStatus);
		{
			WDG_ReloadCounter();
		}
    }// Repetiremos este bucle hasta
    while (lockExecution && retryCounter-- > 0 && *_txStatus != ST_PHY_ACK_RECEIVED);

    return lockExecution && retryCounter >= 0;
}

/**
 * Funcion generica para inducir retrasos en la ejecución
 */
void M2C_Delay(__IO uint32_t nCount)
{
  /* Decrement nCount value */
  while (nCount != 0)
  {
	WDG_ReloadCounter();
    nCount--;
  }
}

/**
 * Inicializa los modulos usados por el procesador (no el de radio)
 */
void M2C_initBoard()
{
	halInit();
	M2C_LEDInit (GLED);
	M2C_LEDInit (RLED);
}

// Funcion de comunicacion serie para imprimir por consola
// Obligatoria para poder compilar con las librerias de SM32, pero no utilizada actualmente
void stSerialPrintf (uint8_t a, char* b) {}
