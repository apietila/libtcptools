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
 * $Id: sessionmanager.c 61 2010-02-21 21:03:03Z salcock $
 *
 */

#define SM_TIMER_QUEUE_LENGTH 100000

#define SM_MODULE_ARRAY_LENGTH 5

#define SM_OUTBOUND 0
#define SM_INBOUND 1

/*
 * This defines how long, in seconds, a session should wait for
 * a SYN/ACK when a SYN has been sent. This is to limit the effect
 * of unsolicited traffic as seen on the trace.
 */
#define SM_TCP_SYN_TIMEOUT 60
#define SM_TIME_WAIT_TIMEOUT 60

#include <stdio.h>
#include <stdlib.h>
#include <libtrace.h>
#include "tcpsession.h"
#include "hashtable.h"
#include "sessionmanager.h"

/*
 * This struct holds the state of a session manager.
 */
struct session_manager_t {
	/*
	 * The hashtable containing the sessions
	 */
	hashtable_t *hashtable;

	/*
	 * The modules registered with the manager to collect statistics
	 */
	struct session_module_t **modules;
	uint8_t module_count;

	/*
	 * This holds sessions in the TIME_WAIT state which is investigated for
	 * deletion once every second. A circular list is used for simplicity.
	 */
	struct timer_queue_t {
		tcp_session_t *sessions[SM_TIMER_QUEUE_LENGTH];
		uint32_t times[SM_TIMER_QUEUE_LENGTH];
		unsigned int lower_idx;
		unsigned int length;
	} waiting_sessions;

	/*
	 * Stores when the last clean of the queue and hashtable occurred.
	 */
	uint32_t last_access;
	uint32_t last_clean;

	/*
	 * Stores the latest closed session. The closed session should be passed
	 * back to the user after the update function is called, so the resources
	 * should only be freed on the next call of the update function.
	 */
	tcp_session_t *closed_session;
};

/*
 * Frees a session, removing it from the hashtable and freeing its module data.
 */
void session_manager_free_session (session_manager_t * manager, tcp_session_t * session);

/*
 * Frees the data associated with the modules for a session.
 */
void session_manager_free_module_data (session_manager_t * manager, tcp_session_t * session);

/*
 * The cleanup routine to remove sessions in the SYN_RCVD or SYN_SENT state
 * due to unsolicited traffic.
 */
void session_manager_cleanup (session_manager_t * manager);

/*
 * Adds a session in the TIME_WAIT state to the queue.
 */
void timer_queue_add (session_manager_t * manager, tcp_session_t * session, uint32_t times);

/*
 * Frees all expired sessions in the TIME_WAIT state.
 */
void timer_queue_free (session_manager_t * manager, uint32_t current_time);

/*
 * If a TCP SYN arrives on a session that is in the TIME_WAIT state, the old
 * session should be freed and a new one created. This function therefore
 * frees a session earlier than its timeout for this purpose.
 */
void timer_queue_free_early (session_manager_t * manager, tcp_session_t * session);



/*
 * Creates and initialises a session manager.
 */
session_manager_t *session_manager_create () {
	int i;

	/* Allocate and initialise memory */

	session_manager_t *manager = (session_manager_t *) malloc (sizeof (session_manager_t));

	manager->hashtable = hashtable_create ();

	manager->modules = (struct session_module_t **) malloc (SM_MODULE_ARRAY_LENGTH * sizeof (struct session_module_t *));

	for (i = 0; i < SM_MODULE_ARRAY_LENGTH; i++)
		manager->modules[i] = NULL;

	manager->module_count = 0;

	manager->waiting_sessions.lower_idx = 0;
	manager->waiting_sessions.length = 0;

	for (i = 0; i < SM_TIMER_QUEUE_LENGTH; i++) {
		manager->waiting_sessions.sessions[i] = NULL;
		manager->waiting_sessions.times[i] = 0;
    }
    
	manager->last_access = 0;

	manager->last_clean = 0;

	manager->closed_session = NULL;

	return manager;
}

/*
 * Frees all memory allocated by this session manager.
 */
void session_manager_destroy (session_manager_t * manager) {

	/* Free sessions and the hashtable entries */
	hashtable_iterator_t *itr = hashtable_iterator_create (manager->hashtable);
	tcp_session_t *session;

	while ((session = hashtable_iterator_next (manager->hashtable, itr)) != NULL) {
		/* Remove entry from hashtable */
		hashtable_iterator_remove (itr);
		/* Free memory associated with session */
		session_manager_free_module_data (manager, session);
		/* Free session itself */
		free (session);
	}
        free(itr);
}

/*
 * Registers a module with this session manager and returns the index of the
 * module. A tcp_session_t will have an array of pointers, and the index will
 * represent the position in the array of the data associated with the
 * registered module.
 */
int session_manager_register_module (session_manager_t * manager, struct session_module_t *module) {

	/* Simply add the module to the simple vector */

	int count = manager->module_count;

	manager->modules[count] = module;

	manager->module_count++;
	count++;

	/* If the list is full, make more room */
	if ((count % SM_MODULE_ARRAY_LENGTH) == 0) {
		count += SM_MODULE_ARRAY_LENGTH;
		manager->modules = (struct session_module_t **) realloc (manager->modules, count * sizeof (struct session_module_t *));
	}

	return manager->module_count - 1;
}

/*
 * This function updates the session to which the packet belongs, and returns
 * the session back to the user. All registerd modules are also updated
 * with the packet.
 */
tcp_session_t *session_manager_update (session_manager_t * manager, struct libtrace_packet_t * packet) {

	int i;
	tcp_session_id_t id;
	struct libtrace_ip *ip;
	struct libtrace_tcp *tcp;
	int direction;

	tcp_session_t *session;

	/* Check if there are any waiting sessions needing to be freed. The
	 * sessions freed here are those in the TIME_WAIT state.
	 */
	uint32_t current_time = (uint32_t) (trace_get_erf_timestamp (packet) >> 32);
	if (current_time != manager->last_access) {
		manager->last_access = current_time;
		timer_queue_free (manager, current_time);
	}
	/* Check if there is a closed session waiting to be freed
	 * We only free a closed session after it has been returned to the user
	 * therefore we need to remember to free it on the next invocation of
	 * the update function.
	 */
	if (manager->closed_session != NULL) {
		if (manager->closed_session->waiting == 0)
			session_manager_free_session (manager, manager->closed_session);
		manager->closed_session = NULL;
	}
	/* Check if a cleanup needs to be performed, which occurs once every
	 * SM_TCP_SYN_TIMEOUT. The purpose of the cleanup is to remove SYNs
	 * that do not have any other matching packets.
	 */
	if (current_time - manager->last_clean > SM_TCP_SYN_TIMEOUT) {
		manager->last_clean = current_time;
		session_manager_cleanup (manager);
	}

	if ((ip = trace_get_ip ((libtrace_packet_t*)packet)) == NULL)
		return NULL;

	if ((tcp = trace_get_tcp ((libtrace_packet_t*)packet)) == NULL)
		return NULL;

	direction = trace_get_direction (packet);

	/* Initialise id. The lowest IP address is used as ip_a, and this
	 * ensures that packets in both directions will be matched to the
	 * same session.
	 */
	if (ip->ip_src.s_addr < ip->ip_dst.s_addr) {
		id.ip_a = ip->ip_src.s_addr;
		id.ip_b = ip->ip_dst.s_addr;
		id.port_a = ntohs (tcp->source);
		id.port_b = ntohs (tcp->dest);
	} else {
		id.ip_a = ip->ip_dst.s_addr;
		id.ip_b = ip->ip_src.s_addr;
		id.port_a = htons (tcp->dest);
		id.port_b = htons (tcp->source);
	}

	/* Find session */
	session = hashtable_retrieve (manager->hashtable, &id);

	/* What follows is the processing of the TCP session state. */

	if (session == NULL) {

		if (tcp->syn && !(tcp->ack)) {
			/* Allocate a new session */
			session = malloc (sizeof (tcp_session_t));

			/* Give it its id */
			session->id.ip_a = id.ip_a;
			session->id.ip_b = id.ip_b;
			session->id.port_a = id.port_a;
			session->id.port_b = id.port_b;

			/* Clear flags */
			session->waiting = 0;

			/* Allocate modules' storage */
			session->data = malloc (manager->module_count * sizeof (void *));
			for (i = 0; i < manager->module_count; i++) {
				session->data[i] = manager->modules[i]->create (session);
			}

			/* Add the session to the hashtable */
			hashtable_insert (manager->hashtable, session);

			/* Change state */
			if (direction == SM_OUTBOUND) {
				/* Outbound, so SYN was sent */
				session->state = SYN_SENT;
				session->expected_ack = ntohl (tcp->seq) + ntohs (ip->ip_len) - ((ip->ip_hl + tcp->doff) << 2);
			} else {
				session->state = SYN_RCVD;
				session->expected_ack = 0xffffffff;
			}
		}
	} else {

		if (tcp->rst) {
			/* TODO: should probably check that RST is valid before
			 * applying it to the current session.
			 */
			session->state = RESET;
			manager->closed_session = session;
		}
		/* Modify state if necessary */
		switch (session->state) {
		case SYN_RCVD:{
				if (direction == SM_OUTBOUND) {
					if (tcp->syn && tcp->ack) {
						/* If a SYN/ACK is sent, the
						 * expected acknowledgement
						 * must be recorded to compare
						 * it against the incoming ACK
						 * packet.
						 */
						session->expected_ack = ntohl (tcp->seq) + ntohs (ip->ip_len) - ((ip->ip_hl + tcp->doff) << 2);
					}
				} else {
					if (tcp->ack && ntohl (tcp->ack_seq) >= session->expected_ack) {
						session->state = ESTABLISHED;
					}
				}
				break;
			}
		case SYN_SENT:{
				if (direction == SM_INBOUND) {
					if (tcp->syn) {
						if (tcp->ack) {
							if (ntohl (tcp->ack_seq) >= session->expected_ack) {
								session->state = ESTABLISHED;
							}
							/* Else invalid ACK,
							 * probably will see a
							 * RST later
							 */
						} else {
							session->state = SYN_RCVD;
						}
					}
				}
				break;
			}
		case ESTABLISHED:{
				if (direction == SM_OUTBOUND && tcp->fin) {
					session->state = FIN_WAIT_1;
					session->expected_ack = ntohl (tcp->seq) + ntohs (ip->ip_len) - ((ip->ip_hl + tcp->doff) << 2);
				} else if (direction == SM_INBOUND && tcp->fin) {
					session->state = CLOSE_WAIT;
				}
				break;
			}
		case FIN_WAIT_1:{
				if (direction == SM_INBOUND) {
					if (tcp->ack && ntohl (tcp->ack_seq) >= session->expected_ack) {
						if (tcp->fin) {
							session->state = TIME_WAIT;
							session->waiting = 1;
							timer_queue_add (manager, session, current_time);
						} else {
							session->state = FIN_WAIT_2;
						}
					} else {
						if (tcp->fin) {
							session->state = CLOSING;
						}
					}
				}
				break;
			}
		case FIN_WAIT_2:{
				if (direction == SM_INBOUND && tcp->fin) {
					session->state = TIME_WAIT;
					session->waiting = 1;
					timer_queue_add (manager, session, current_time);
				}
				break;
			}
		case CLOSING:{
				if (direction == SM_INBOUND && tcp->ack && ntohl (tcp->ack_seq) >= session->expected_ack) {
					session->state = TIME_WAIT;
					session->waiting = 1;
					timer_queue_add (manager, session, current_time);
				}
				break;
			}
		case TIME_WAIT:{
				if (tcp->syn) {
					/* Need to free the session and start a
					 * new one 
					 */
					timer_queue_free_early (manager, session);
					return session_manager_update (manager, packet);
				}
				break;
			}
		case CLOSE_WAIT:{
				if (direction == SM_OUTBOUND && tcp->fin) {
					session->expected_ack = ntohl (tcp->seq) + ntohs (ip->ip_len) - ((ip->ip_hl + tcp->doff) << 2);
					session->state = LAST_ACK;
				}
				break;
			}
		case LAST_ACK:{
				if (direction == SM_INBOUND && tcp->ack && ntohl (tcp->ack_seq) >= session->expected_ack) {
					session->state = CLOSED;
					manager->closed_session = session;
				}
				break;
			}
		case CLOSED:{
				break;
			}
		case RESET:{
				break;
			}
		default:{
				printf ("Error\n");
			}
		}
	}

	/* If the session is valid, update the associated modules */
	if (session != NULL) {
		session->last_access = current_time & 0xff;
		for (i = 0; i < manager->module_count; i++) {
			manager->modules[i]->update (session->data[i], packet);
		}
	}

	return session;
}

/*
 * Frees a session, removing it from the hashtable and freeing its module data.
 */
void session_manager_free_session (session_manager_t * manager, tcp_session_t * session) {

	/* Free modules' data */
	int i;
	for (i = 0; i < manager->module_count; i++) {
		manager->modules[i]->destroy (session->data[i]);
		session->data[i] = NULL;
	}

	free (session->data);
	session->data = NULL;

	/* Remove from hashtable */
	hashtable_remove (manager->hashtable, &(session->id));

	/* Free session itself */
	free (session);
	session = NULL;
}

/*
 * Frees the data associated with the modules for a session.
 */
void session_manager_free_module_data (session_manager_t * manager, tcp_session_t * session) {

	int i;

	if (session->waiting == 1)
		return;

	for (i = 0; i < manager->module_count; i++) {
		manager->modules[i]->destroy (session->data[i]);
	}
	free (session->data);
	session->data = NULL;
}

/*
 * The cleanup routine to remove sessions in the SYN_RCVD or SYN_SENT state
 * due to unsolicited traffic.
 */
void session_manager_cleanup (session_manager_t * manager) {

	/* Iterate through hashtable, freeing any session in SYN_RECEIVED or
	 * SYN_SENT and not accessed within 60 seconds
	 */
	hashtable_iterator_t *itr = hashtable_iterator_create (manager->hashtable);
	tcp_session_t *session;
	uint8_t last_access = (manager->last_access & 0xff);
	uint8_t difference;

	while ((session = hashtable_iterator_next (manager->hashtable, itr)) != NULL) {

		if (session->state == SYN_RCVD || session->state == SYN_SENT) {
			difference = last_access - session->last_access;
			if (difference > SM_TCP_SYN_TIMEOUT) {
				/* Free session and entry in hash table */
				hashtable_iterator_remove (itr);
				session_manager_free_module_data (manager, session);
				free (session);
				session = NULL;
			}
		}
	}
        free(itr);
        
}

/*
 * Adds a session in the TIME_WAIT state to the queue.
 */
void timer_queue_add (session_manager_t * manager, tcp_session_t * session, uint32_t time) {

	struct timer_queue_t *queue = &(manager->waiting_sessions);

	int pos = queue->lower_idx + queue->length;
	if (pos >= SM_TIMER_QUEUE_LENGTH)
		pos -= SM_TIMER_QUEUE_LENGTH;

	queue->sessions[pos] = session;
	queue->times[pos] = time;

	queue->length++;
	if (queue->length > SM_TIMER_QUEUE_LENGTH) {
		fprintf (stderr, "Timer queue length exceeded. Please increase SM_TIMER_QUEUE_LENGTH\n");
	}
}

/*
 * Frees all expired sessions in the TIME_WAIT state
 */
void timer_queue_free (session_manager_t * manager, uint32_t current_time) {
	/* Find time below which sessions should be freed */
	struct timer_queue_t *queue;
	tcp_session_t *session;
	current_time -= SM_TIME_WAIT_TIMEOUT;
	queue = &(manager->waiting_sessions);

	while ((queue->times[queue->lower_idx] < current_time) && (queue->length != 0)) {

		session = queue->sessions[queue->lower_idx];

		/* printf("TIME_WAIT expired for: (%8x:%5u , %8x:%5u)\n", session->id.ip_a, session->id.port_a, session->id.ip_b, session->id.port_b); */

		/* Check for sessions already freed */
		if (session != NULL)
			session_manager_free_session (manager, session);

		queue->sessions[queue->lower_idx] = NULL;

		queue->lower_idx++;
		queue->length--;
		if (queue->lower_idx == SM_TIMER_QUEUE_LENGTH) {
			queue->lower_idx = 0;
		}
	}
}

/*
 * If a TCP SYN arrives on a session that is in the TIME_WAIT state, the old
 * session should be freed and a new one created. This function therefore
 * frees a session earlier than its timeout for this purpose.
 */
void timer_queue_free_early (session_manager_t * manager, tcp_session_t * session) {

	struct timer_queue_t *queue = &(manager->waiting_sessions);
	unsigned int i, pos;
	for (i = 0; i < queue->length; i++) {

		pos = queue->lower_idx + i;
		if (pos >= SM_TIMER_QUEUE_LENGTH)
			pos -= SM_TIMER_QUEUE_LENGTH;

		if (queue->sessions[pos] == NULL)
			continue;

		/* If session is in list, set it to NULL so it doesn't get freed again */
		if (tcp_session_id_equals (&(queue->sessions[pos]->id), &(session->id))) {
			queue->sessions[pos] = NULL;
			session_manager_free_session (manager, session);
			break;
		}

	}

}
