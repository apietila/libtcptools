/*
 * This file is part of libtcptools
 *
 * Copyright (c) 2009 The University of Waikato, Hamilton, New Zealand.
 * Authors: Brett McGirr 
 * 	    Shane Alcock
 *          
 * All rights reserved.
 *
 * This code has been developed by the University of Waikato WAND 
 * research group. For further information please see http://www.wand.net.nz/
 *
 * libtcptools is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * libtcptools is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libtcptools; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: bwest.c 60 2009-05-13 03:59:37Z salcock $
 *
 */


/*
 * The code here was copied from Perry's, but changed into C and made
 * to fit into the module framework.
 * 
 * 
 */

#include <libtrace.h>
#include <stdlib.h>
#include <stdio.h>
#include "sessionmanager.h"
#include "bwest.h"

/*
 * This struct stores the necessary data for recording the RTT of a handshake.
 */
struct bwest_t {
	uint64_t bytesin;
	uint64_t bytesout;

	uint32_t ackin;
	uint32_t ackout;

	uint8_t established;
};

/*
 * Allocates and initialises a new data structure for a new tcp session.
 */
void *bwest_create () {
	struct bwest_t *record = malloc (sizeof (struct bwest_t));
	record->bytesin = 0;
	record->bytesout = 0;
	record->ackin=0;
	record->ackout=0;
	record->established = 0;
	return record;
}

/*
 * Frees the data structure of a closed tcp session.
 */
void bwest_destroy (void *data) {
	free (data);
}

/*
 * Updates the RTT estimates given a new packet belonging to the flow.
 */
void bwest_update (void *data, struct libtrace_packet_t *packet) {

	struct bwest_t *record = (struct bwest_t *) data;
	struct libtrace_tcp *tcp = trace_get_tcp((struct libtrace_packet_t *)packet);

	int direction = trace_get_direction(packet);

	if (direction !=0 && direction != 1)
		return;

	if (record->established) {
		if (direction==0) {
			/* outgoing */
			uint32_t len=htonl(tcp->ack_seq);
			len=len-record->ackout;
			record->bytesin+=len;
			record->ackout=htonl(tcp->ack_seq);
		} else {
			/* incoming */
			uint32_t len=htonl(tcp->ack_seq);
			len=len-record->ackin;
			record->bytesout+=len;
			record->ackin=htonl(tcp->ack_seq);
		}
	} else if (tcp->syn && tcp->ack) {
		if (direction==0) {
			/* outgoing */
			record->ackin=htonl(tcp->seq);
			record->ackout=htonl(tcp->ack_seq);
		} else  {
			/* incoming */
			record->ackout=htonl(tcp->seq);
			record->ackin=htonl(tcp->ack_seq);
		}
		record->established=1;
	}

}

/*
 * This returns the session module for use by the session manager.
 */
struct session_module_t *bwest_module () {
	struct session_module_t *module = malloc (sizeof (struct session_module_t));
	module->create = &bwest_create;
	module->destroy = &bwest_destroy;
	module->update = &bwest_update;
	return module;
}

/*
 * Return the bytes incoming
 */
uint64_t bwest_incoming (void *data) {
	struct bwest_t *record = (struct bwest_t *) data;
	return record->bytesin;
}

/*
 * Return the bytes outgoing
 */
uint64_t bwest_outgoing (void *data) {
	struct bwest_t *record = (struct bwest_t *) data;
	return record->bytesout;
}

/*
 * Return the total bytes.
 */
uint64_t bwest_total (void *data) {
	return bwest_incoming(data)+bwest_outgoing(data);
}

