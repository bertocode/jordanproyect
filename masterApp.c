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

#define SIZE_OF_IMAGE 256
uint32_t image[SIZE_OF_IMAGE] =
{
	0x20002000, 0x080053F5, 0x08005291, 0x08005293, //5000
	0x08005295, 0x08005297, 0x08005299, 0x00000000, //5010
	0x00000000, 0x00000000, 0x00000000, 0x0800529B, //5020
	0x0800529D, 0x00000000, 0x0800529F, 0x080052A1, //5030
	0x08005449, 0x0800544B, 0x08006291, 0x0800544F, //5040
	0x08005451, 0x080052A5, 0x08005455, 0x08005457, //5050
	0x08008181, 0x08007AEF, 0x08008439, 0x080066F5, //5060
	0x080052A7, 0x08005463, 0x08005465, 0x08005467, //5070
	0x08005469, 0x08005447, 0x4C0EB538, 0xF0216821, //5080
	0xEA41417F, 0x20016500, 0xF9FCF004, 0xFA2AF004, //5090
	0xF0042035, 0x4620FA71, 0xFBD6F004, 0x46204629, //50A0
	0xFB8CF004, 0xFA2AF004, 0x4038E8BD, 0xF0042000, //50B0
	0xBF00B9E9, 0x0800FC00, 0x8F4FF3BF, 0x4B054A04, //50C0
	0xF40168D1, 0x430B61E0, 0xF3BF60D3, 0xE7FE8F4F, //50D0
	0xE000ED00, 0x05FA0004, 0x4604B537, 0xF0022001, //50E0
	0xF44FFFEE, 0xF0025080, 0x2000FFEE, 0xFFF2F002, //50F0
	0xA8012102, 0xF828F003, 0xF0019801, 0x2001F891, //5100
	0xFFECF002, 0xF0022000, 0x4605FFAF, 0xE7FEB100, //5110
	0xFFCDF002, 0xF0022001, 0x2001FFCE, 0xFFE2F002, //5120
	0xFFA7F002, 0xF0022008, 0x200BFFBE, 0xFFA5F002, //5130
	0x70632361, 0x70A32388, 0x5380F44F, 0x80A370E5, //5140
	0x80E32302, 0x72232301, 0xBD30B003, 0x43F8E92D, //5150
	0x46154680, 0x460C461E, 0x1C17B119, 0x2701BF18, //5160
	0x460FE000, 0xB10DB2FF, 0x702B2300, 0x2301B10C, //5170
	0x46407023, 0xFF8BF002, 0xF0044681, 0xF1B9FC1D, //5180
	0xD1F00F00, 0x7823B117, 0xD1FC2B00, 0xFC14F004, //5190
	0x3E01B15F, 0xB21EB2B3, 0xB21B3301, 0xDD022B00, //51A0
	0x2B8F782B, 0x43F7D1DF, 0xB2F80FFF, 0x83F8E8BD, //51B0
	0x9001B507, 0xB12B9B01, 0xFBFEF004, 0x3B019B01, //51C0
	0xE7F79301, 0xF85DB003, 0xB508FB04, 0xFFE4F000, //51D0
	0xF0002000, 0xE8BDF807, 0x20014008, 0xB802F000, //51E0
	0x00004770, 0x4B09B573, 0xF8334E09, 0x46045010, //51F0
	0xF8562301, 0x46690020, 0x3004F88D, 0xF0049500, //5200
	0xF856FB53, 0x611D3024, 0xBD70B002, 0x08009B98, //5210
	0x20000000, 0x4A044B03, 0x3020F853, 0x2010F832, //5220
	0x4770611A, 0x20000000, 0x08009B98, 0x4A044B03, //5230
	0x3020F853, 0x2010F832, 0x4770615A, 0x20000000, //5240
	0x08009B98, 0x49054B04, 0x3020F853, 0x1010F831, //5250
	0x404A68DA, 0x477060DA, 0x20000000, 0x08009B98, //5260
	0xB5380049, 0xB2CC4605, 0x4628B14C, 0xFFEAF7FF, //5270
	0xF6473C01, 0xF7FF70FF, 0xB2E4FF9B, 0xBD38E7F4, //5280
	0xE7FE4770, 0xE7FEE7FE, 0x4770E7FE, 0x47704770, //5290
	0xB936F000, 0x47704770, 0xF7FFB508, 0x4806FF96, //52A0
	0xFF1AF7FF, 0xF7FF2001, 0x2000FFB5, 0xFFBEF7FF, //52B0
	0x22004B02, 0xBD08601A, 0x20000258, 0x20000254, //52C0
	0x4C0BB510, 0x726323F3, 0x781B4B0A, 0x230F72A3, //52D0
	0x78E37023, 0xB2DB3301, 0xF00470E3, 0x2100FB6D, //52E0
	0x460A4620, 0xE8BD460B, 0xF7FF4010, 0xBF00BF2F, //52F0
	0x20000258, 0x0800FC00, 0x4604B510, 0x46012001, //5300
	0xFFAEF7FF, 0xD0002C8F, 0x2000B91C, 0xF7FF2101, //5310
	0x4B03FFA7, 0x701C2200, 0x701A4B02, 0xBF00BD10, //5320
	0x20000347, 0x200002C6, 0x2101B538, 0x20004604, //5330
	0xFF96F7FF, 0x78134A07, 0x7821B95B, 0xB2DB1C58, //5340
	0xD3044299, 0x49045CE5, 0x460354CD, 0x2301E7F5, //5350
	0xBD387013, 0x20000348, 0x200002C7, 0xF7FFB538, //5360
	0x4D1BFF9B, 0xB32B782B, 0x7A5A4B1A, 0xD11F2AF3, //5370
	0x7A9B4A19, 0xF0226812, 0x429A427F, 0x2000D218, //5380
	0xFF54F7FF, 0xF7FF2001, 0x2406FF51, 0xF7FF2000, //5390
	0x2001FF59, 0xFF56F7FF, 0x48103C01, 0xFF08F7FF, //53A0
	0x04FFF014, 0x4620D1F2, 0xFE66F7FF, 0xFE84F7FF, //53B0
	0x702B2300, 0xF0044C0A, 0x6823FAFF, 0xF7FFB923, //53C0
	0xF06FFF7F, 0xE001437F, 0x3B016823, 0xE7C86023, //53D0
	0x20000348, 0x200002C7, 0x0800FC00, 0x0003FFFF, //53E0
	0x20000254, 0x4A0B490A, 0x429A4B0B, 0xF851BFBE, //53F0
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

	M2C_sendPacket_Locking(&rf_tx_packet, &waitingForTxRadioStatus, &txStatus, M2C_RETRY_A_LOT);
}

void masterSendNodeFirmware(uint16_t dest)
{
    __IO uint32_t packet_buff_size = 0;
    uint32_t packet_index = 0;
    uint32_t image_index = 0;
    uint8_t MAX_PACKET_DATA_SIZE = 100;

    while (image_index < SIZE_OF_IMAGE)
    {
        // -- Comenzamos llendado el buffer del paquete que vamos a enviar
        // Rellenamos las cabeceras
        // Identificamos el paquete como datos de actualizacion de FW
        bufferTx[0] = M2C_PACKET_TYPE_CONTAINS_FW;
        // Indicadicador de tamano, (se completa mas tarde)
        bufferTx[1] = 0x00;
        bufferTx[2] = USER_NODE_VERSION;
        // Indicador de inicio/final de FW, este dato se actualiza mas tarde
        bufferTx[3] = 0x00;

        // Actualizamos el espacio del paquete usado
        packet_buff_size += 4;

        /* Explicacion de la comprobacion del bucle:
            ((4 <= (MAX_PACKET_DATA_SIZE - packet_buff_size)) && (image_index < sizeof(image)))
            Antes de comenzar cada vuelta comprobamos si en el buffer tenemos espacio para los otros
            4 nuevos bytes que queremos meter ( de ahi el -4).
            Ademas comprobamos si aun estamos dentro de los limites del tamaño de la imagen con la que estamos trabajando
        */
        while ((4 <= (MAX_PACKET_DATA_SIZE - packet_buff_size)) && (image_index < SIZE_OF_IMAGE))
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

        // Comprobamos si es el primer paquete del firmware
        if (packet_index <= 0)
        	bufferTx[3] = M2C_FIRST_PACKET_MARKER;

        // hacemos la comprobacion de lo que nos queda por enviar DESPUES de haber preparado el paquete para su envio
        if (image_index >= SIZE_OF_IMAGE)
            bufferTx[3] = M2C_LAST_PACKET_MARKER;

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
        if (!M2C_sendPacket_Locking(&rf_tx_packet, &waitingForTxRadioStatus, &txStatus, M2C_RETRY_A_LOT))
        	break;

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
