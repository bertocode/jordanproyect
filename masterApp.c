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

boolean packetRecived;								//Flag para indicar que hay un paquete recibido pendiente de ser procesado en el bucle main
__IO uint8_t radioBufferRx[M2C_RADIO_RX_BUFFER];	// Buffer donde se almacenan los paquetes recibidos para su posterior procesado
__IO uint8_t radioBufferTx[M2C_RADIO_TX_BUFFER];	// Buffer donde se almacenan los datos para su posterior transmisión


UART_InitTypeDef UART_InitStructure;
SC_DMA_InitTypeDef SC_DMA_InitStructure;

#define SERIAL_RX_BUFFER_LENGTH 21
#define SERIAL_TX_BUFFER_LENGTH 4
uint8_t serialTxBuffer[SERIAL_TX_BUFFER_LENGTH];
uint8_t serialRxBuffer[SERIAL_RX_BUFFER_LENGTH];

__IO uint16_t version;

// Timers -----
uint32_t timerLedBlink;

void initBoard(void)
{
	M2C_initBoard();
	M2C_radioInit(&rf_tx_packet);

	M2C_LEDOff (GLED);
	M2C_LEDOn (RLED);

	packetRecived = 0;

	// UART INIT
	UART_InitStructure.UART_BaudRate = 115200;
	UART_InitStructure.UART_WordLength = UART_WordLength_8b;
	UART_InitStructure.UART_StopBits = UART_StopBits_1;
	UART_InitStructure.UART_Parity = UART_Parity_Even;
	UART_InitStructure.UART_HardwareFlowControl = UART_HardwareFlowControl_Disable;

	//STM_EVAL_COMInit(COM1, &UART_InitStructure);
	GPIO_InitTypeDef GPIO_InitStructure;

	/* Disable the serial controller interface */
	UART_Cmd(SC1_UART, DISABLE);

	/* UART configuration: Set up the parameters specific for the UART operating mode */
	UART_Init(SC1_UART, &UART_InitStructure);

	/* Configure UART Tx as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/* Set pull-up on UART Tx pin */
	GPIO_SetBits(GPIOB, GPIO_Pin_1);

	/* Configure UART Rx as input with pull-up */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	/* Set pull-up on UART Rx pin */
	GPIO_SetBits(GPIOB, GPIO_Pin_2);

	/* Enable the UART peripheral */
	UART_Cmd(SC1_UART, ENABLE);

	/* Reset DMA Channel Tx */
	SC_DMA_ChannelReset(SC1_DMA, DMA_ChannelReset_Tx);
	/* SC1 DMA channel Tx config */
	SC_DMA_InitStructure.DMA_BeginAddrA = (uint32_t)serialTxBuffer;
	SC_DMA_InitStructure.DMA_EndAddrA = (uint32_t)(serialTxBuffer + SERIAL_TX_BUFFER_LENGTH);
	SC_DMA_Init(SC1_DMA_ChannelTx, &SC_DMA_InitStructure);

	/* Reset DMA Channel Rx */
	SC_DMA_ChannelReset(SC1_DMA, DMA_ChannelReset_Rx);
	/* SC1 DMA channel Rx config */
	SC_DMA_InitStructure.DMA_BeginAddrA = (uint32_t)serialRxBuffer;
	SC_DMA_InitStructure.DMA_EndAddrA = (uint32_t)(serialRxBuffer + SERIAL_RX_BUFFER_LENGTH);
	SC_DMA_Init(SC1_DMA_ChannelRx, &SC_DMA_InitStructure);

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
    // Anadimos la version actual al paquete
    rf_tx_packet.data[1] = (0x00FF & version);
    rf_tx_packet.data[2] = (0xFF00 & version) >> 8;
    rf_tx_packet.dest_short_addr = dest;

	rf_tx_packet.len = M2C_RADIO_HEADER_SIZE + 3 + M2C_RADIO_TAIL_SIZE;
	rf_tx_packet.seq++;
	WDG_ReloadCounter();

	M2C_sendPacket_Locking(&rf_tx_packet, &waitingForTxRadioStatus, &txStatus, M2C_RETRY_A_LOT);
}

/**
 * Envia la linea pasada como parametro a al nodo hijo dest.
 *
 * La función devuelve TRUE si se ha enviado correctamente. FALSE si se han
 * agotado los reintentos de envio.
 */
int masterSendNodeHexFileLine(uint16_t dest, uint8_t line[])
{
	// Reiniciamos la longitud del paquete
	uint8_t size = 0;

	// Ponemos la cabecera al paquete
    rf_tx_packet.data[0] = M2C_PACKET_TYPE_CONTAINS_FW;

    // Anadimos la version actual al paquete
    rf_tx_packet.data[1] = (0x00FF & version);
    rf_tx_packet.data[2] = (0xFF00 & version) >> 8;

    // Slots de la cabecera no usados
    rf_tx_packet.data[3] = 0x00;

    size += 4; // Longitud de nuestra cabecera

    // Metemos los datos que nos dan al paquete que vamos a transmitir
    uint32_t i;
    for (i=0; i < SERIAL_RX_BUFFER_LENGTH; i++) // TODO: Este limite habrá que cambiarlo
    {
    	rf_tx_packet.data[4+i] = line[i];
    	size++;
    }

    // Seleccionamos el destino
    rf_tx_packet.dest_short_addr = dest;
    // Sumamos la longitud de cabecras y colas de cada paquete
	rf_tx_packet.len = M2C_RADIO_HEADER_SIZE + size + M2C_RADIO_TAIL_SIZE;
	rf_tx_packet.seq++;

	return M2C_sendPacket_Locking(&rf_tx_packet, &waitingForTxRadioStatus, &txStatus, M2C_RETRY_A_LOT);
}

/**
 * Pide una linea del archivo hex de firmware a través de la UART.
 *
 * Este metodo obtiene la linea indicada por el numero pasado en el serialTxBuffer
 */
void masterSendNodeFirmware(uint16_t dest)
{
	uint8_t max_radio_data_buffer_size = M2C_RADIO_MAX_DATA_SIZE - 4; // Restamos 4 por nuestra propia cabecera de control
	uint8_t radio_data_buffer_size = 0;

	do
	{
		// Preparamos el buffer de recepcion serie
		SC_DMA_ChannelLoadEnable(SC1_DMA, DMA_ChannelLoad_ARx);

		// Escribimos algo en el puerto serie a la espera de que la Rpi nos conteste
		SC_DMA_ChannelLoadEnable(SC1_DMA, DMA_ChannelLoad_ATx);
		while (SC_DMA_GetFlagStatus(SC1_DMA, DMA_FLAG_TXAACK) == SET)
		  WDG_ReloadCounter();

		// Esperamos a recibir la linea del archivo HEX
		while (SC_DMA_GetFlagStatus(SC1_DMA, DMA_FLAG_RXAACK) == SET)
		  WDG_ReloadCounter();

		HexLine* incoming_hexline = (HexLine*)&serialRxBuffer;

		radio_data_buffer_size +=  sizeof(HexLine);

		// TODO: Almacenar serialRxBuffer en un buffer mayor
		if (incoming_hexline->type == END_OF_FILE_RECORD)
			break;
	}
	while(radio_data_buffer_size < max_radio_data_buffer_size);

	masterSendNodeHexFileLine(dest, serialRxBuffer);
}

/**
 * Obtiene la version de firmware disponible pidiendolo por la UART.
 *
 * Esta version será guardad y se utilizará como valor de version para otras acciones
 * hasta que se vuelva a invocar a esta funcion.
 */
uint16_t masterGetVersionFromUART(void)
{
	// Preparamos el buffer de recepcion serie
	SC_DMA_ChannelLoadEnable(SC1_DMA, DMA_ChannelLoad_ARx);

	// Escribimos en el puerto serie a la espera de que la Rpi nos conteste
	serialTxBuffer[0] = 0xB0;
	SC_DMA_ChannelLoadEnable(SC1_DMA, DMA_ChannelLoad_ATx);
	while (SC_DMA_GetFlagStatus(SC1_DMA, DMA_FLAG_TXAACK) == SET)
	  WDG_ReloadCounter();

	// Esperamos a recibir el id de version
	while (SC_DMA_GetFlagStatus(SC1_DMA, DMA_FLAG_RXAACK) == SET)
	  WDG_ReloadCounter();

	version = *((uint16_t*)&serialRxBuffer[0]);
	return version;
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
			radioBufferRx[i] = packet[i];

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
			RadioPacket* rPacket = (RadioPacket*) radioBufferRx;

		    switch (rPacket->data[0])
		    {
		    	case M2C_PACKET_TYPE_FW_EXCHANGE_VERSION:
		    		packetRecived = FALSE;
		    		masterGetVersionFromUART();
		    		masterSendNodeFirmwareVersion(rPacket->src_short_addr);
		    		break;
		    	case M2C_PACKET_TYPE_WAITING_FOR_FW:
		    		serialTxBuffer[0] = 0xA0;

		    		serialTxBuffer[1] = rPacket->data[2];
		    		serialTxBuffer[2] = rPacket->data[3];
		    		serialTxBuffer[3] = rPacket->data[4];
		    		serialTxBuffer[4] = rPacket->data[5];

		    		packetRecived = FALSE;
		    		masterSendNodeFirmware(rPacket->src_short_addr);
		    		break;
		    	default:
		    		// No se que hacer con este paquete, despreciado.
		    		packetRecived = FALSE;
		    		break;
		    }
		}

		if (timerLedBlink <= 0)
		{
			M2C_LEDToggle(RLED);
			timerLedBlink = M2C_DELAY_LONG;
		}
		else
			timerLedBlink--;

		WDG_ReloadCounter();
	}
}
