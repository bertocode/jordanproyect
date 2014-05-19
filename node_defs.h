/*
 * node_defs.h
 *
 *  Created on: 17/05/2014
 *      Author: barbosa
 */
// Definicion de variables propias de cada nodo
#ifndef NODE_DEFS_H_
#define NODE_DEFS_H_

#ifdef MASTER_NODE
	// ID del nodo (radio)
	#define USER_NODE_ID		(uint16_t)0x2
	// ID del nodo con el que nos comunicaremos (radio)
	#define USER_GW_ID			(uint16_t)0x1
	// ID del la red en la que volcaremos mensajes de radio (radio)
	#define USER_PAN_ID			(uint16_t)0x1000
	// Version actual del nodo. En realidad la leemos de memoria
	// Solo esta aqui por si se necesita hardcodear en algun momento
	#define USER_NODE_VERSION 	2
#else
	// ID del nodo (radio)
	#define USER_NODE_ID		(uint16_t)0x1
	// ID del nodo con el que nos comunicaremos (radio)
	#define USER_GW_ID			(uint16_t)0x2
	// ID del la red en la que volcaremos mensajes de radio (radio)
	#define USER_PAN_ID			(uint16_t)0x1000
	// Version actual del nodo. En realidad la leemos de memoria
	// Solo esta aqui por si se necesita hardcodear en algun momento
	#define USER_NODE_VERSION 	((*((uint32_t*) CONFIG_MASK_ADDRESS)) & 0x00FFFFFF)
#endif

#endif /* NODE_DEFS_H_ */
