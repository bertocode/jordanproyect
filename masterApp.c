#ifndef MASTER_NODE
	#define MASTER_NODE
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32w108xx.h"
#include "system_stm32w108xx.h"
#include "node_defs.h"
#include "m2c_defs.h"
#include "bootloader.h"

/**
 * VARIABLES RELACIONADAS CON EL MODULO DE RADIO
 **/
RadioPacket rf_tx_packet;				// Instancia unica de la estructura de paquetes a enviar
__IO StStatus txStatus;					// Resultado de la emision (ACK recibido, ACK no recibido...)
__IO boolean waitingForTxRadioStatus;	// Flag para indicar si estamos esperando a que txStatus esté disponible

boolean packetRecived;					//Flag para indicar que hay un paquete recibido pendiente de ser procesado en el bucle main
__IO uint8_t bufferRx[128];				// Buffer donde se almacenan los paquetes recibidos para su posterior procesado
__IO uint8_t bufferTx[128];				// Buffer donde se almacenan los datos para su posterior transmisión

uint32_t image[256] =
{
		0x20002000,
		0xfacebeef,
};

void initBoard(void)
{
	M2C_initBoard();
	M2C_radioInit(&rf_tx_packet);

	M2C_LEDOff (GLED);
	M2C_LEDOn (RLED);

	packetRecived = 0;
}

/*
 * Este metodo envia la version de firmware a un nodo hijo. Esta llamada bloquea la ejecución
 * hasta haberse asegurado de que el paquete ha sido recibido por el nodo destino.
 */
void masterSendNodeFirmwareVersion(uint16_t dest)
{
	// Ponemos la cabecera al paquete
    rf_tx_packet.data[0] = M2C_PACKET_TYPE_FW_EXCHANGE_VERSION;
    // Anadimos la version actual al paquete
    rf_tx_packet.data[1] = USER_NODE_VERSION;
    rf_tx_packet.dest_short_addr = dest;

	rf_tx_packet.len = M2C_RADIO_HEADER_SIZE + 2 + M2C_RADIO_TAIL_SIZE;
	rf_tx_packet.seq++;
	WDG_ReloadCounter();

	M2C_sendPacket_Locking(&rf_tx_packet, &waitingForTxRadioStatus, &txStatus);
}

void masterSendNodeFirmware(uint16_t dest)
{
    __IO uint32_t packet_buff_size = 0;
    uint32_t packet_index = 0;
    uint32_t image_index = 0;
    uint8_t MAX_PACKET_DATA_SIZE = 100;
    uint16_t page_offset = 0;

    while (image_index < sizeof(image))
    {
        if (image_index > 0 && image_index % FLASH_PAGE_SIZE / 4 == 0)
            page_offset++;

        // -- Comenzamos llendado el buffer del paquete que vamos a enviar
        // Rellenamos las cabeceras
        // Identificamos el paquete como datos de actualizacion de FW
        bufferTx[0] = M2C_PACKET_TYPE_CONTAINS_FW;
        // Indicadicador de tamano, (se completa mas tarde)
        bufferTx[1] = 0x00;
        bufferTx[2] = USER_NODE_VERSION;
        // Indicador de final de FW, este dato se actualiza mas tarde
        bufferTx[3] = 0x00;

        // Actualizamos el espacio del paquete usado
        packet_buff_size += 4;

        /* Explicacion de la comprobacion del bucle:
            ((4 <= (MAX_PACKET_DATA_SIZE - packet_buff_size)) && (image_index < sizeof(image)))
            Antes de comenzar cada vuelta comprobamos si en el buffer tenemos espacio para los otros
            4 nuevos bytes que queremos meter ( de ahi el -4).
            Ademas comprobamos si aun estamos dentro de los limites del tamaño de la imagen con la que estamos trabajando
        */
        while ((4 <= (MAX_PACKET_DATA_SIZE - packet_buff_size)) && (image_index < sizeof(image)))
        {
            uint8_t part0 = (image[image_index] & 0x000000FF);
            uint8_t part1 = (image[image_index] & 0x0000FF00) >> 8;
            uint8_t part2 = (image[image_index] & 0x00FF0000) >> 16;
            uint8_t part3 = (image[image_index] & 0xFF000000) >> 24;

            bufferTx[packet_buff_size + 0] = part3;
            bufferTx[packet_buff_size + 1] = part2;
            bufferTx[packet_buff_size + 2] = part1;
            bufferTx[packet_buff_size + 3] = part0;

            packet_buff_size += 4;
            image_index++;
        }

        // hacemos la comprobacion de lo que nos queda por enviar DESPUES de haber preparado el paquete para su envio
        if (image_index >= sizeof(image))
            bufferTx[3] = 0xFF;

        // anadimos el tamano de los datos a nuestra cabecera del paquete
        bufferTx[1] = packet_buff_size - 4;

        // -- A partir de aqui asumimos que tenemos el buffer del paquete a enviar lleno

        // copiamos el buffer de un lado a otro
        uint8_t i;
        for (i = 0; i < sizeof(bufferTx); i++)
                rf_tx_packet.data[i] = bufferTx[i];

        // -- A partir de aqui esta todo listo para enviar
        // Definimos el destino y el tamano
        rf_tx_packet.dest_short_addr = dest;
        rf_tx_packet.len = M2C_RADIO_HEADER_SIZE + packet_buff_size + M2C_RADIO_TAIL_SIZE;

        // Enviamos el paquete
        M2C_sendPacket_Locking(&rf_tx_packet, &waitingForTxRadioStatus, &txStatus);

        // incrementamos el contador de paquete
        packet_index++;
        // reiniciamos el tamaño del buffer del paquete para la siguiente vuelta
        packet_buff_size = 0;
    }
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
		    		masterSendNodeFirmwareVersion(rPacket->src_short_addr);
		    		break;
		    	case M2C_PACKET_TYPE_WAITING_FOR_FW:
		    		masterSendNodeFirmware(rPacket->src_short_addr);
		    		break;
		    	default:
		    		// No se que hacer con este paquete, despreciado.
		    		 break;
		    }

			packetRecived = FALSE;
		}

		M2C_LEDToggle(RLED);
		M2C_Delay(M2C_DELAY_LONG);
	}
}
