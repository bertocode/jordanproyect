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
__IO boolean flashing_finished       = FALSE;			// Flag para indicar si se ha terminado de grabar en flash

/**
 * VARIABLES RELACIONADAS CON EL MODULO DE RADIO
 **/
RadioPacket rf_tx_packet;				// Instancia unica de la estructura de paquetes a enviar
__IO StStatus txStatus;					// Resultado de la emision (ACK recibido, ACK no recibido...)
__IO boolean waitingForTxRadioStatus;	// Flag para indicar si estamos esperando a que txStatus esté disponible

__IO boolean packetRecived;					//Flag para indicar que hay un paquete recibido pendiente de ser procesado en el bucle main
__IO uint8_t radioBufferRx[M2C_RADIO_RX_BUFFER];			// Buffer donde se almacenan los paquetes recibidos para su posterior procesado
// No se utiliza buffer de transmision en modo BL ya que solo se envian paquetes de beacon (cortos)
//__IO uint8_t bufferTx[128];				// Buffer donde se almacenan los datos para su posterior transmisión
__IO uint32_t line_index;

/**
 * TIMERS
 */
__IO uint32_t nodeGetMasterFirmwareTimer; // Timer para controlar cada cuanto pedimos el firmware al nodo master

/**
 * Este metodo envia la petición de version de firmware al nodo padre. Esta llamada bloquea la ejecución
 * hasta haberse asegurado de que el paquete ha sido recibido por el nodo destino.
 *
 * El parametro first determina si este es el primer paquete que se envia al nodo maestro o es uno de los
 * acks enviados entre orden y orden.
 */
void nodeIsReadyToGetNewFirm(boolean first)
{
    rf_tx_packet.data[0] = M2C_PACKET_TYPE_WAITING_FOR_FW;
    rf_tx_packet.data[1] = first ? 0x00 : 0xFF;

    rf_tx_packet.data[2] = (0x000000FF & line_index);
    rf_tx_packet.data[3] = (0x0000FF00 & line_index) >> 8;
    rf_tx_packet.data[4] = (0x00FF0000 & line_index) >> 16;
    rf_tx_packet.data[5] = (0xFF000000 & line_index) >> 24;

	rf_tx_packet.len = M2C_RADIO_HEADER_SIZE + 6 + M2C_RADIO_TAIL_SIZE;
	rf_tx_packet.seq++;

	M2C_sendPacket_Locking(&rf_tx_packet, &waitingForTxRadioStatus, &txStatus, M2C_RETRY_A_LOT);
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
			radioBufferRx[i] = packet[i];

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
		nodeGetMasterFirmwareTimer = 0;

		M2C_LEDOn(GLED);
		M2C_LEDOn(RLED);

		// Activamos la escritura en memoria
		FPEC_ClockCmd(ENABLE);
		FLASH_Unlock();

		uint16_t currentAddress;
		uint32_t startAddress = 0;
		line_index = 0;

		while (!flashing_finished)
		{
			// Timeout para pedir el firmware al master si llevamos un rato sin recibir nada
			if (nodeGetMasterFirmwareTimer <= 0)
			{
				nodeIsReadyToGetNewFirm(TRUE);
				nodeGetMasterFirmwareTimer = M2C_DELAY_LONG;
			}
			else
				nodeGetMasterFirmwareTimer--;

			if (packetRecived)
			{
				// Si nos llega un paquete actualizamos el timer de espera para pedir fw
				nodeGetMasterFirmwareTimer = M2C_DELAY_LONG;

				RadioPacket* rPacket = (RadioPacket*) radioBufferRx;

				switch (rPacket->data[0])
				{
					case M2C_PACKET_TYPE_CONTAINS_FW:
					{
						__IO HexLine* hexLine = (HexLine*) &rPacket->data[4];

						switch(hexLine->type)
						{
							case DATA_RECORD:
								currentAddress = hexLine->offset;

								uint16_t* data16 = (uint16_t*) hexLine->data;
								uint32_t targetAddress = currentAddress + startAddress;

								// Borramos la página si estamos en el inicio de una
								if ((targetAddress >= APPLICATION_ADDRESS && targetAddress < FLASH_END_ADDR)
										&& targetAddress % FLASH_PAGE_SIZE == 0)
									FLASH_ErasePage(targetAddress);

								uint16_t i;
								for(i=0; i < ( hexLine->numBytes >> 1 ); i++)
									FLASH_ProgramHalfWord(targetAddress + i*2, data16[i]);

								currentAddress += (uint16_t) hexLine->numBytes;
								break;
							case END_OF_FILE_RECORD:
								M2C_setNodeVersion(rPacket->data[1]);
								M2C_setBootMode(BOOT_MODE_APP);
								flashing_finished = 1;
								break;
							case EXTENDED_LINEAR_ADDRESS_RECORD:
								startAddress = ((hexLine->data[0] << 8) + (hexLine->data[1])) << 16;
								break;
							default:
								break;
						}

						// Aumentamos el contador de linea de página para la siguiente vez
						line_index++;

						// Marcamos aqui el paquete como procesado ya que no lo vamos a usar
						// y marcarlo luego provoca errores en las llamadas de interrupcion
						packetRecived = FALSE;

						if (hexLine->type != END_OF_FILE_RECORD)
							nodeIsReadyToGetNewFirm(FALSE);
						break;
					}
					default:
						// No se que hacer con este paquete, despreciado.
						packetRecived = FALSE;
						break;
				}
			}

			WDG_ReloadCounter();
		}

		// Volvemos a bloquear la flash. No es necesario pero si correcto.
		FLASH_Lock();
		FPEC_ClockCmd(DISABLE);

		// Saltamos a la aplicación
		M2C_jumpToApplication();
		return 1; // Esto en realidad no hace falta, pero nos curamos en salud
	}
}
