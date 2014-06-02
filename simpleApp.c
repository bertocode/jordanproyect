/* Includes ------------------------------------------------------------------*/
#include "stm32w108xx.h"
#include "system_stm32w108xx.h"
#include "node_defs.h"
#include "m2c_defs.h"
#include "bootloader.h"
#include "led.h"

/**
 * VARIABLES RELACIONADAS CON EL MODULO DE RADIO
 **/
RadioPacket rf_tx_packet;				// Instancia unica de la estructura de paquetes a enviar
__IO StStatus txStatus;					// Resultado de la emision (ACK recibido, ACK no recibido...)
__IO boolean waitingForTxRadioStatus;	// Flag para indicar si estamos esperando a que txStatus esté disponible

boolean packetRecived;						//Flag para indicar que hay un paquete recibido pendiente de ser procesado en el bucle main
__IO uint8_t bufferRx[M2C_RADIO_RX_BUFFER];	// Buffer donde se almacenan los paquetes recibidos para su posterior procesado
//__IO uint8_t bufferTx[128];				// Buffer donde se almacenan los datos para su posterior transmisión

/**
 * TIMERS
 */
__IO uint32_t nodeGetMasterFirmwareVersionTimer; // Timer para controlar cada cuanto pedimos el firmware al nodo master

/**
 * Redefinimos el offset de la posicion de la tabla de los vectores de interrupcion.
 * Al estar en la aplicacion estamos en la seccion de memoria alta, y por tanto usamos la
 * tabla de vectores de interrupcion movida tanto como ocupe el bootloader.
 *
 * NOTA: El nombre de esta variable viene impuesto por STM32, NO MODIFICARLO.
 */
#define VECT_TAB_OFFSET APPLICATION_OFFSET

void initBoard(void)
{
	M2C_initBoard();
	M2C_radioInit(&rf_tx_packet);

	M2C_LEDOn (RLED);
	M2C_LEDOff (GLED);

	nodeGetMasterFirmwareVersionTimer = 0;
}

/*
 * Este metodo envia la petición de version de firmware al nodo padre. Esta llamada bloquea la ejecución
 * hasta haberse asegurado de que el paquete ha sido recibido por el nodo destino.
 */
void nodeGetMasterFirmwareVersion(void)
{
    rf_tx_packet.data[0] = M2C_PACKET_TYPE_FW_EXCHANGE_VERSION;
    rf_tx_packet.data[1] = USER_NODE_VERSION;

	rf_tx_packet.len = M2C_RADIO_HEADER_SIZE + 1 + M2C_RADIO_TAIL_SIZE;
	rf_tx_packet.seq++;
	WDG_ReloadCounter();

	// Lo enviamos, pero realmente no nos preocupamos de si llega o no
	// ya que no es prioritario mientras sigamos en modo aplicacion
	M2C_sendPacket(&rf_tx_packet);
}

/**
 * Callback de la libreria de radio de STM32.
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
	initBoard();

	while (TRUE)
	{
		if (packetRecived)
		{
			RadioPacket* rPacket = (RadioPacket*) bufferRx;

			switch (rPacket->data[0])
			{
				case M2C_PACKET_TYPE_FW_EXCHANGE_VERSION:
				{
					// El nodo maestro nos ha enviado su version
					// Comprobamos si es mayor que la nuestra.
					uint16_t remoteVer = *((uint16_t*)&rPacket->data[1]);
					if (USER_NODE_VERSION < remoteVer)
					{
						// Hacemos una senalizacion led de que vamos a entrar en modo bootloader
						// (Tres parpadeos de los dos leds a la vez)
						M2C_LEDOff(GLED);
						M2C_LEDOff(RLED);

						uint8_t i;
						for (i=0; i<6; i++)
						{
							M2C_LEDToggle(GLED);
							M2C_LEDToggle(RLED);
							M2C_Delay(M2C_DELAY_LONG);
						}

						// Cambiamos el modo de arranque y saltamos al BL
						M2C_setBootMode((uint8_t)BOOT_MODE_BL);// tenemos nuevo FW
						M2C_jumpToBootloader();
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

		if (nodeGetMasterFirmwareVersionTimer <= 0)
		{
			nodeGetMasterFirmwareVersion();
			nodeGetMasterFirmwareVersionTimer = M2C_DELAY_VLONG;
		}
		else
			nodeGetMasterFirmwareVersionTimer--;

	}
}
