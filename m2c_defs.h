#ifndef __M2C_DEFS
#define __M2C_DEFS

#include "stm32w108xx.h"
#include "stm32w108xx_wdg.h"
#include "gnu.h"
#include "error.h"
#include "micro-common.h"
#include "phy-library.h"
#include "led.h"
#include "node_defs.h"
#include "bootloader.h"

// Posicion de inicio de la aplicacion
// IMPORATE QUE ESTE DEFINIDA PARA COMPILAR TANTO BL COMO APP
#define APPLICATION_OFFSET  ((uint32_t)0x5000)

// Definiciones relacionadas con la parte de radio
#define M2C_RADIO_POWER 			8
#define M2C_RADIO_CHANNEL			11

#define M2C_RADIO_MAX_DATA_SIZE		100
#define M2C_RADIO_HEADER_SIZE		12
#define M2C_RADIO_TAIL_SIZE			2

/*
 * La funcionalidad de esta cabecera depende del emisor.
 *
 * Si el emisor es el nodo maestro, esta type irá en la cabecera del paquete
 * que contiene la información de version.
 *
 * Si el emisor es un nodo hijo este type irá en el paquete de peticion de
 * version de FW al nodo maestro.
 *
 */
#define M2C_PACKET_TYPE_FW_EXCHANGE_VERSION			 0xF3

/*
 * La funcionalidad de esta cabecera depende del emisor.
 *
 * Si el emisor es el nodo maestro, esta type ira en todas
 * las cabeceras de los paquetes enviados que contengan información de la
 * nueva imagen de firmware que se esta enviado.
 *
 * Si el emisor es un nodo hijo, el nodo ha detectado que hay un nuevo FW, se ha reiniciado,
 * ha entrado en el modo bootloader y esta esperando que el nodo maestro le mande los
 * paquetes que contienen la actualizacion de FW.
 *
 * NOTA: Se define dos veces el mismo valor para facilitar la lectura del codigo en
 * ambos lados (nodo hijo y nodo maestro)
 */
#define M2C_PACKET_TYPE_WAITING_FOR_FW 		0xFA
#define M2C_PACKET_TYPE_CONTAINS_FW 		M2C_PACKET_TYPE_WAITING_FOR_FW

// Estructura de paquete de radio
struct RadioPacket_ {
	uint8_t len;
	uint8_t fcf1;
	uint8_t fcf2;
	uint8_t seq;
	uint16_t dest_pan_id;
	uint16_t dest_short_addr;
	uint8_t src_short_addr;
	uint8_t data[M2C_RADIO_MAX_DATA_SIZE];
};
__IO typedef struct RadioPacket_ RadioPacket;
// puntero a funcion
typedef int (*pFunction)(void);

// Definicion de delays unica
#define M2C_DELAY_SHORT 	0x007FFF
#define M2C_DELAY_LONG 		0x03FFFF
#define M2C_DELAY_VLONG		0xFFFFFF

// Definicion de numero de retrys
#define M2C_RETRY_A_FEW    5
#define M2C_RETRY_A_LOT    20

// Funciones a exportar
// Relacionadas con el bootloader
void M2C_setBootMode(uint8_t bootMode);
void M2C_setNodeVersion(uint8_t version);
void M2C_jumpToBootloader(void);
void M2C_jumpToApplication(void);
// Relacionadas con la radio
void M2C_radioInit(RadioPacket* rPacket);
boolean M2C_sendPacket_Locking(RadioPacket* rPacket, __IO boolean* _waitingForTxRadioStatus, __IO StStatus* _txStatus, int16_t retryCounter);
#define M2C_sendPacket(A) M2C_sendPacket_Locking(A, NULL, NULL, 0)
// Genericas
void M2C_initBoard(void);
void M2C_Delay(__IO uint32_t nCount);

#endif // __M2C_DEFS
