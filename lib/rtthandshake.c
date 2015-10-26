/*
 * This file is part of libtcptools
 *
 * Copyright (c) 2009 The University of Waikato, Hamilton, New Zealand.
 * Authors: Brett McGirr 
 *          Shane Alcock
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
 * $Id: rtthandshake.c 60 2009-05-13 03:59:37Z salcock $
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
#include "rttmodule.h"
#include "rtthandshake.h"

/*
 * This struct stores the necessary data for recording the RTT of a handshake.
 */
struct rtt_handshake_record_t {
	// The rtt for the inside part of the session
	double rtt_in;

	// The rtt for the outside part of the session
	double rtt_out;

	// Records if a session has been established or not
	uint8_t established;
};

/*
 * Allocates and initialises a new data structure for a new tcp session.
 */
void *rtt_handshake_create () {
	struct rtt_handshake_record_t *record = malloc (sizeof (struct rtt_handshake_record_t));
	record->rtt_in = -1.0;
	record->rtt_out = -1.0;
	record->established = 0;
	return record;
}

/*
 * Frees the data structure of a closed tcp session.
 */
void rtt_handshake_destroy (void *data) {
	free (data);
}

/*
 * Updates the RTT estimates given a new packet belonging to the flow.
 */
void rtt_handshake_update (void *data, struct libtrace_packet_t *packet) {

	struct rtt_handshake_record_t *record = (struct rtt_handshake_record_t *) data;
	struct libtrace_tcp *tcp;

	double time;
	int direction;

	// If a session has been established then skip all calculations.
	if (!record->established) {

		if ((tcp = trace_get_tcp (packet)) == NULL)
			return;

		time = trace_get_seconds (packet);
		direction = trace_get_direction (packet);

		// Check that the direction is ok
		if(!(direction==0 || direction==1))
			return;


		// Check if the packet is a SYN, a SYN/ACK or an ACK
		if (tcp->syn) {
			if (tcp->ack) {

				// If the SYN/ACK is a retransmit, then we must not
				// update the rtt to the origin of the SYN/ACK but 
				// only to the destination.
				if (direction == 0) {	//outbound, so incoming syn
					if(record->rtt_in < 0.0) { // Do not update if rtt already set
						record->rtt_in += time;
					}
					record->rtt_out = -time;
				} else {
					if(record->rtt_out < 0.0) {
						record->rtt_out += time;
					}
					record->rtt_in = -time;
				}

			} else {
				if(direction == 0) {
					record->rtt_out = -time;
				} else {
					record->rtt_in = -time;
				}
			}
		} else if (tcp->ack) {
			// ack - syn_ack gives the time for the other direction
			if (direction == 0) {	//outbound, so incoming syn_ack
				record->rtt_in += time;
			} else {
				record->rtt_out += time;
			}
			record->established = 1;
		}

	}

}

/*
 * This returns the session module for use by the session manager.
 */
struct session_module_t *rtt_handshake_module () {
	struct session_module_t *module = malloc (sizeof (struct session_module_t));
	module->create = &rtt_handshake_create;
	module->destroy = &rtt_handshake_destroy;
	module->update = &rtt_handshake_update;
	return module;
}

/*
 * This returns the rtt module for use by the reordering module.
 */
struct rtt_module_t *rtt_handshake_rtt_module () {
	struct rtt_module_t *module = malloc (sizeof (struct rtt_module_t));
	module->session_module.create = &rtt_handshake_create;
	module->session_module.destroy = &rtt_handshake_destroy;
	module->session_module.update = &rtt_handshake_update;
	module->inside_rtt = &(rtt_handshake_inside);
	module->outside_rtt = &(rtt_handshake_outside);
	return module;
}

/*
 * Return the total RTT.
 */
double rtt_handshake_total (void *data) {
	struct rtt_handshake_record_t *record = (struct rtt_handshake_record_t *) data;
	// If both in and out rtt are valid then return the sum,
	// otherwise return -1.0 indicating an error.
	if (record->established == 0)
		return -1.0;
	else
		return record->rtt_in + record->rtt_out;
}

/*
 * Return the RTT for the inside half of the connection.
 */
double rtt_handshake_inside (void *data) {
	struct rtt_handshake_record_t *record = (struct rtt_handshake_record_t *) data;
	if(record->rtt_in > 0.0)
		return record->rtt_in;
	else
		return -1.0;
}

/*
 * Return the RTT for the outside half of the connection.
 */
double rtt_handshake_outside (void *data) {
	struct rtt_handshake_record_t *record = (struct rtt_handshake_record_t *) data;
	if(record->rtt_out > 0.0)
		return record->rtt_out;
	else
		return -1.0;
}
