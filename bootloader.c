#include "bootloader.h"

#ifndef APPLICATION_OFFSET
	#error "APPLICATION_OFFSET not defined"
#endif

/**
 * Redefinimos el offset de la posicion de la tabla de los vectores de interrupcion.
 * Al estar en el bootloader estamos en la seccion de memoria baja, y por tanto usamos la
 * tabla de vectores de interrupcion base.
 *
 * NOTA: El nombre de esta variable viene impuesto por STM32, NO MODIFICARLO.
 */
#define VECT_TAB_OFFSET 0

/**
 * VARIABLES RELACIONADAS CON LA ESCRITURA EN FLASH
 **/
#define PAGE_TO_FLASH_BUFFER_SIZE FLASH_PAGE_SIZE / 4	// Tamano de página en words (uint32_t)
uint32_t pageToFlashBuffer[PAGE_TO_FLASH_BUFFER_SIZE];	// Buffer donde se guardan las paginas recibidas por radio para su posterior grabacion en flash
__IO uint16_t pageToFlashBufferIndex = 0;				// Indice de llenado del buffer pageToFlashBuffer
__IO uint16_t page_index             = 0;				// Indice de la ultima pagina escrita en flash
__IO boolean flashing_finished       = FALSE;			// Flag para indicar si se ha terminado de grabar en flash

/**
 * VARIABLES RELACIONADAS CON EL MODULO DE RADIO
 **/
RadioPacket rf_tx_packet;				// Instancia unica de la estructura de paquetes a enviar
__IO StStatus txStatus;					// Resultado de la emision (ACK recibido, ACK no recibido...)
__IO boolean waitingForTxRadioStatus;	// Flag para indicar si estamos esperando a que txStatus esté disponible

boolean packetRecived;					//Flag para indicar que hay un paquete recibido pendiente de ser procesado en el bucle main
__IO uint8_t bufferRx[128];				// Buffer donde se almacenan los paquetes recibidos para su posterior procesado
// No se utiliza buffer de transmision en modo BL ya que solo se envian paquetes de beacon (cortos)
//__IO uint8_t bufferTx[128];				// Buffer donde se almacenan los datos para su posterior transmisión

/**
 * Vacia la página asociada a la direccion address y escribe el
 * contenido del array data pasado como parametro en dicha pagina.
 *
 * El array data debe tener estrictamente FLASH_PAGE_SIZE/4 posiciones, tener
 * mas de estos hará que se ignoren; tener menos posiblemente provoque un
 * error o que se escriban datos no deseados.
 */
void writeFlashPage(uint32_t address, uint32_t data[])
{
	FPEC_ClockCmd(ENABLE);
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP| FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

	FLASH_ErasePage(address);

	uint16_t i;
	for (i=0; i < FLASH_PAGE_SIZE/4; i++)
		FLASH_ProgramWord(address + 4*i, data[i]);

	FLASH_Lock();
	FPEC_ClockCmd(DISABLE);
}

/*
 * Este metodo envia la petición de version de firmware al nodo padre. Esta llamada bloquea la ejecución
 * hasta haberse asegurado de que el paquete ha sido recibido por el nodo destino.
 */
void nodeIsReadyToGetNewFirm(void)
{
    rf_tx_packet.data[0] = M2C_PACKET_TYPE_WAITING_FOR_FW;

	rf_tx_packet.len = M2C_RADIO_HEADER_SIZE + 1 + M2C_RADIO_TAIL_SIZE;
	rf_tx_packet.seq++;

	M2C_sendPacket_Locking(&rf_tx_packet, &waitingForTxRadioStatus, &txStatus);
}

/**
 * Callback de la libreria de radio de STM32.
 *
 * Esta función se llama cada vez que un paquete termina de enviarse.
 * En nuestro caso solo almacenamos el valor del status y marcamos el flag
 * de respuesta recibida.
 */
void ST_RadioTransmitCompleteIsrCallback(StStatus status, uint32_t sfdSentTime, boolean framePending)
{
	M2C_LEDBlink(RLED, 1);

	if (status == ST_PHY_ACK_RECEIVED || status == ST_SUCCESS)
		M2C_LEDBlink(GLED, 1);

	txStatus = status;
	waitingForTxRadioStatus = FALSE;
}

/**
 * Este metodo es llamado cada vez que el nodo recibe un paquete.
 *
 * Copiamos el paquete al buffer para su uso por el bucle main
 * siempre y cuando no haya otro procesandose
 */
void ST_RadioReceiveIsrCallback(uint8_t *packet,
        boolean ackFramePendingSet,
        uint32_t time,
        uint16_t errors,
        int8_t rssi)
{
	M2C_LEDBlink(GLED, 1);

	if (!packetRecived)
	{
		uint8_t i;
		for (i=0; i<=packet[0]; i++)
			bufferRx[i] = packet[i];

		packetRecived = TRUE;
	}
}

int main(void)
{
	if (BOOT_MODE & BOOT_MODE_APP)
	{
		// Estamos en modo aplicacion, hacemos el salto a memoria y nada más.
		// De las rutinas de inicializacion ya se encargará la aplicación a donde vamos
		M2C_jumpToApplication();
		return 1; // Esto en realidad no hace falta, pero nos curamos en salud
	}
	else //if (BOOT_MODE & BOOT_MODE_BL)
	{
		// Inicializamos la placa en modo completo
		M2C_initBoard();
		M2C_radioInit(&rf_tx_packet);
		NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0);

		M2C_LEDOn(GLED);
		M2C_LEDOn(RLED);

		// Pedimos de forma insitente al nodo maestro que nos envie el nuevo firmware.
		nodeIsReadyToGetNewFirm();

		while (!flashing_finished)
		{
			if (packetRecived)
			{
				RadioPacket* rPacket = (RadioPacket*) bufferRx;

				switch (rPacket->data[0])
				{
					case M2C_PACKET_TYPE_CONTAINS_FW:
					{
						// La condicion esta dividida por 4 porque queremos trabajar con uint32, no uint8
						uint16_t i;
						for (i = 1; i < rPacket->data[1] / 4; i++)
						{
							// Reconstruimos el uint32 a partir de 4 bytes
							uint32_t wordToBuffer = rPacket->data[4*i + 0] << 24 | rPacket->data[4*i + 1] << 16 | rPacket->data[4*i + 2] << 8 | rPacket->data[4*i + 3];
							pageToFlashBuffer[pageToFlashBufferIndex] = wordToBuffer;
							pageToFlashBufferIndex++;

							// Si tenemos el buffer lleno o es nuestro ultimo paquete
							if (pageToFlashBufferIndex == PAGE_TO_FLASH_BUFFER_SIZE - 1 || rPacket->data[3] == M2C_LAST_PACKET_MARKER)
							{
								// Escribimos la pagina en flash
								writeFlashPage(APPLICATION_ADDRESS + page_index*FLASH_PAGE_SIZE, pageToFlashBuffer);
								// Reiniciamos el buffer para prepararlo para la siguiente pagina
								pageToFlashBufferIndex = 0;
								// Incrementamos el indice de página
								page_index++;
							}

							// Si era nuestro ultimo paquete de FW..
							if (rPacket->data[3] == M2C_LAST_PACKET_MARKER)
							{
								// Borramos la flag de arranque en BL
								M2C_setBootMode(BOOT_MODE_APP);
								M2C_setNodeVersion(rPacket->data[3]);
								flashing_finished = 1;
							}
						}
						break;
					}
					default:
						// No se que hacer con este paquete, despreciado.
						 break;
				}

		    	packetRecived = 0;
			}

			WDG_ReloadCounter();
		}

		M2C_jumpToApplication();
		return 1; // Esto en realidad no hace falta, pero nos curamos en salud
	}
}
