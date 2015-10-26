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
 * $Id: rtttimestamp.c 60 2009-05-13 03:59:37Z salcock $
 *
 */


/*
 * The code here was copied from Perry's, but changed into C and made
 * to fit into the module framework.
 */

/*
 * Things to note:
 * 
 * Measurement is discarded if:
 * 		estimate > 20s
 * 
 * Smoothing function rtt = (new + rtt*3) /4
 */

#include <stdlib.h>
#include <stdio.h>
#include <libtrace.h>
#include "sessionmanager.h"
#include "rttmodule.h"
#include "queue.h"
#include "rtttimestamp.h"

#define RTT_MULT 5
#define MAX_RTT 20
#define SMOOTH 0.75

#define DATA_PACKETS_ONLY 1

// This allows our queue of timestamp/time pairs to grow indefinitely.
struct queue_vars_t rtt_timestamp_queue_vars = { -1, 0, 0 };

/*
 * This struct is an item of the queue. We need to store the timestamps
 * along with the time at which the data packet arrived.
 */
struct rtt_timestamp_item_t {
	uint32_t timestamp;
	double time;
};

/*
 * This struct keeps track of the average rtt over the session and also
 * stores the timestamp/time queues for both directions.
 */
struct rtt_timestamp_t {
	struct queue_t *queue[2];
	double estimates[2];
	double totals[2];
	int counts[2];
};

/*
 * Allocates and initialises a new data structure for a new tcp session.
 */
void *rtt_timestamp_create () {
	struct rtt_timestamp_t *rtt_data = malloc (sizeof (struct rtt_timestamp_t));
	int i;

	// Guess that an increment of 10 will do
	rtt_timestamp_queue_vars.buffer_increment = 10;
	rtt_timestamp_queue_vars.item_size = sizeof (struct rtt_timestamp_item_t);

	// Initialise the variables for both directions.
	for (i = 0; i < 2; i++) {
		rtt_data->queue[i] = queue_create (&rtt_timestamp_queue_vars);
		rtt_data->estimates[i] = -1.0;
		rtt_data->counts[i] = 0;
		rtt_data->totals[i] = 0.0;
	}
	return rtt_data;
}

/*
 * Frees the data structure of a closed tcp session.
 */
void rtt_timestamp_destroy (void *data) {
	struct rtt_timestamp_t *rtt_data = (struct rtt_timestamp_t *) data;
	queue_destroy (rtt_data->queue[0]);
	queue_destroy (rtt_data->queue[1]);
	free (rtt_data);
}

/*
 * Updates the RTT estimates given a new packet belonging to the flow.
 */
void rtt_timestamp_update (void *data, struct libtrace_packet_t *packet) {

	struct rtt_timestamp_t *rtt_data = (struct rtt_timestamp_t *) data;

	struct libtrace_tcp *tcpptr = trace_get_tcp (packet);
	struct libtrace_ip *ipptr = trace_get_ip (packet);

	struct queue_itr_t itr;
	struct queue_t *queue;
	struct rtt_timestamp_item_t *item;

	double now, diff;

	unsigned char *pkt = NULL;
	int plen;
	unsigned char type = 0, optlen = 0, *optdata = NULL;
	
	int direction = trace_get_direction (packet);
	int reverse = 1 - direction;

	// Check that the direction is ok
	if(!(direction==0 || direction==1))
		return;

	now = trace_get_seconds (packet);

	// Search for the timestamp option.
	pkt = (unsigned char *) tcpptr + sizeof (*tcpptr);
	plen = (tcpptr->doff * 4 - sizeof *tcpptr);

	while (trace_get_next_option (&pkt, &plen, &type, &optlen, &optdata)) {
		uint32_t *ts = NULL;
		uint32_t *tsecho = NULL;

		// Ignore non timestamp options
		if (type != 8) {
			continue;
		}

		ts = (uint32_t *) & optdata[0];
		tsecho = (uint32_t *) & optdata[4];

		// Look for timestamp of reverse direction of which this is an echo
		queue = rtt_data->queue[reverse];
		item = queue_itr_begin (queue, &rtt_timestamp_queue_vars, &itr);
		while (item != NULL) {
			if (*tsecho > item->timestamp) {

				// Remove elements from queue.
				queue_itr_remove (queue, &itr);
				item = queue_itr_next (queue, &rtt_timestamp_queue_vars, &itr);

			} else if (*tsecho == item->timestamp) {

				// Update RTT.
				diff = now - item->time;
				if (diff < MAX_RTT) {
					// Record value for average measurement
					rtt_data->totals[reverse] += diff;
					rtt_data->counts[reverse]++;

					if (rtt_data->estimates[reverse] == -1.0) {
						rtt_data->estimates[reverse] = diff;
					} else {	// smooth
						if(RTT_MULT) {
							if(rtt_data->estimates[reverse]*5 < diff)
								rtt_data->estimates[reverse] = (SMOOTH * rtt_data->estimates[reverse]) + ((1 - SMOOTH) * diff);
						} else {
							rtt_data->estimates[reverse] = (SMOOTH * rtt_data->estimates[reverse]) + ((1 - SMOOTH) * diff);
						}

					}
				}
				break;

			} else {

				break;
			}
		}

		// Add this packet's timestamp to the queue
		if (*ts) {

			if(DATA_PACKETS_ONLY) {
				if((ntohs (ipptr->ip_len) - ((ipptr->ip_hl + tcpptr->doff) << 2)) == 0) {
					return;
				}
			}

			// Search for item in queue
			queue = rtt_data->queue[direction];
			item = queue_itr_begin (queue, &rtt_timestamp_queue_vars, &itr);
			while (item != NULL) {
				// If item is found, update time and then break
				if (item->timestamp == *ts) {
					item->time = now;
					break;
				} else {
					item = queue_itr_next (queue, &rtt_timestamp_queue_vars, &itr);
				}
			}

			if (item == NULL) {
				// Not found, so add new timestamp
				struct rtt_timestamp_item_t new_item;
				new_item.time = now;
				new_item.timestamp = *ts;
				queue_add (queue, &rtt_timestamp_queue_vars, &new_item);
			}
		}
	}
}

/*
 * Return the total RTT.
 */
double rtt_timestamp_total (void *data) {
	struct rtt_timestamp_t *rtt_data = (struct rtt_timestamp_t *) data;

	if (rtt_data->estimates[1] > 0.0 && rtt_data->estimates[0] > 0.0)
		return rtt_data->estimates[1] + rtt_data->estimates[0];
	else
		return -1.0;
}

/*
 * Return the RTT for the inside half of the connection.
 */
double rtt_timestamp_inside (void *data) {
	struct rtt_timestamp_t *rtt_data = (struct rtt_timestamp_t *) data;

	if (rtt_data->estimates[1] > 0.0)
		return rtt_data->estimates[1];
	else
		return -1.0;
}

/*
 * Return the RTT for the outside half of the connection.
 */
double rtt_timestamp_outside (void *data) {
	struct rtt_timestamp_t *rtt_data = (struct rtt_timestamp_t *) data;

	if (rtt_data->estimates[0] > 0.0)
		return rtt_data->estimates[0];
	else
		return -1.0;
}

/*
 * Return the average RTT over the duration of the session
 */
double rtt_timestamp_average (void *data) {
	struct rtt_timestamp_t *rtt_data = (struct rtt_timestamp_t *) data;

	if ((rtt_data->totals[1] > 0.0) && (rtt_data->totals[0] > 0.0)) {
		return (rtt_data->totals[0] / rtt_data->counts[0]) + (rtt_data->totals[1] / rtt_data->counts[1]);
	} else {
		return -1.0;
	}
}

/*
 * This returns the session module for use by the session manager.
 */
struct session_module_t *rtt_timestamp_module () {
	struct session_module_t *session_module = (struct session_module_t *) malloc (sizeof (struct session_module_t));
	session_module->create = &rtt_timestamp_create;
	session_module->destroy = &rtt_timestamp_destroy;
	session_module->update = &rtt_timestamp_update;

	return session_module;
}

/*
 * This returns the rtt module for use by the reordering module.
 */
struct rtt_module_t *rtt_timestamp_rtt_module () {
	struct rtt_module_t *module = malloc (sizeof (struct rtt_module_t));
	module->session_module.create = &rtt_timestamp_create;
	module->session_module.destroy = &rtt_timestamp_destroy;
	module->session_module.update = &rtt_timestamp_update;
	module->inside_rtt = &(rtt_timestamp_inside);
	module->outside_rtt = &(rtt_timestamp_outside);
	return module;
}
