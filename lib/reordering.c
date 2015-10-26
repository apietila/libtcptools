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
 * $Id: reordering.c 60 2009-05-13 03:59:37Z salcock $
 *
 */



#include <libtrace.h>
#include <stdlib.h>
#include <stdio.h>
#include "sessionmanager.h"
#include "rttmodule.h"
#include "reordering.h"

#define REORDERING_ARRAY_INCREMENT 20

#define RTT_FACTOR 0.9
#define RTO_FACTOR 2.0

/* The module used for the RTT calculation */
struct rtt_module_t *rtt_module;

/* Allows useful output of the reordering */
const char *reordering_messages[] = {
	"packet in order",			/* 0 */
	"sequence number higher than expected",
	"unneeded retransmission (packet record not found)",	/* 2 */
	"unneeded retransmission (already acked)",
	"retransmission (cannot find dup acks)",	/* 4 */

	"retransmission (IP ID different)",
	"retransmission (time_lag > rto)",	/* 6 */
	"retransmission (duplicate acks >= 3)",
	"retransmission (in recovery)",	/* 8 */
	"network duplicate",

	"unknown",					/* 10 */
	"network reordering"
};

/*
 * This struct keeps the information about one packet.
 */
struct packet_record_t {
	/* The sequence number of the packet */
	uint32_t seq;

	/* The time the packet was sent */
	double time;

	/* The IP ID of the packet */
	uint16_t ip_id;

	/* The number of times that this packet has been acknowledged */
	uint8_t num_acks;

	/* Holds if this record was created as a placeholder for a missing packet */
	unsigned int  is_missing:1;

	/* Holds if any misalignment occurs */
	unsigned int is_misaligned:1;
	unsigned int padding:6;

	/* Points to extra missing packets when they arrive. The problem that
	 * this linked list solves is the following. Suppose packet 10 arrives,
	 * size 10, followed by packet 30, size 10. The missing packet may be
	 * packet 20 size 10 OR packet 20 and 25 both size 5. In the latter
	 * case it is necessary to have a placeholder for two packets although
	 * this is not known in advance.
	 */
	struct packet_record_t *missing_link;
};

/*
 * This struct holds the details for one half of the connection.
 */
struct sender_record_t {
	/* The array of packet records, which is a queue implemented 
	 * as an array to save on malloc() calls and copying operations.
	 */
	struct packet_record_t *array;

	/* The lower_idx and length are array positions for the queue. */
	uint16_t lower_idx;
	uint16_t length;

	/* The array size is the storage available in the pointer array. */
	uint16_t array_size;

	/* This is the next sequence number expected, and it allows a quick
	 * check as to whether a packet is in sequence, too low or too high.
	 */
	uint32_t expected_seq;

	/* This is used to see if the sender is in a recovery mode. */
	uint8_t in_recovery;
};

/*
 * This struct keeps information about a session.
 */
struct reordering_t {
	/* The packets from each half of the connection. */
	struct sender_record_t record[2];

	/* For RTT module */
	void *rtt_data;

	/* Holds the minimum rtt */
	double min_rtt;

	/* For meaningful output of the last packet. */
	reordering_type_t last_packet;
	int last_packet_message;
	double time_lag;
};


/*
 * Frees the missing links of a packet.
 */
void packet_record_free_missing_links (struct packet_record_t *packet);

/*
 * Adds a new record to the array of packet records.
 */
struct packet_record_t *sender_record_add (struct sender_record_t *record, uint32_t seq, double time, uint16_t ip_id);

/*
 * Acknowledges as many packets in the array as possible, freeing 
 * those acknowledged.
 */
void sender_record_ack (struct sender_record_t *record, uint32_t ack);

/*
 * Finds and returns the packet record given a sequence number.
 */
struct packet_record_t *sender_record_find (struct sender_record_t *record, uint32_t seq);

/*
 * Frees the missing links of a packet.
 */
void packet_record_free_missing_links (struct packet_record_t *packet) {
	/*printf ("Free %8x\n", packet->seq); */

	if (packet->missing_link != NULL) {
		packet_record_free_missing_links (packet->missing_link);
		free (packet->missing_link);
		packet->missing_link = NULL;
	}
}

/*
 * Adds a new record to the array of packet records.
 */
struct packet_record_t *sender_record_add (struct sender_record_t *record, uint32_t seq, double time, uint16_t ip_id) {

	int idx, i;
	struct packet_record_t *new_array = NULL;

	/* Check if there is space in the array */
	if (record->length == record->array_size) {

		/* No space, so need to increase the buffer. */

		record->array_size += REORDERING_ARRAY_INCREMENT;
		new_array = malloc (record->array_size * sizeof (struct packet_record_t));

		i = 0;
		idx = record->lower_idx;

		/* Copy over old array starting from pos 0 */
		while (i < record->length) {

			new_array[i].ip_id = record->array[idx].ip_id;
			new_array[i].is_misaligned = record->array[idx].is_misaligned;
			new_array[i].is_missing = record->array[idx].is_missing;
			new_array[i].missing_link = record->array[idx].missing_link;
			new_array[i].num_acks = record->array[idx].num_acks;
			new_array[i].padding = record->array[idx].padding;
			new_array[i].seq = record->array[idx].seq;
			new_array[i].time = record->array[idx].time;

			i++;
			idx = (idx + 1) % record->length;
		}

		/* Initialise empty elements of the array */
		for (i = record->length; i < record->array_size; i++) {
			new_array[i].missing_link = NULL;
			new_array[i].seq = 0;
		}

		free (record->array);
		record->array = new_array;
		record->lower_idx = 0;
	}
	/* Find the position to add the new packet record */
	idx = record->lower_idx + record->length;
	if (idx >= record->array_size)
		idx -= record->array_size;

	/* Update */
	record->array[idx].num_acks = 0;
	record->array[idx].seq = seq;
	record->array[idx].time = time;
	record->array[idx].ip_id = ip_id;
	record->array[idx].is_missing = 0;
	record->array[idx].missing_link = NULL;

	record->length++;

	return &(record->array[idx]);
}

/*
 * Acknowledges as many packets in the array as possible, freeing 
 * those acknowledged.
 */
void sender_record_ack (struct sender_record_t *record, uint32_t ack) {
	/* Increase the lower idx to point to the highest packet that can be
	 * acknowledged by this ACK
	 */
	uint16_t old_lower_idx;
	struct packet_record_t *packet = NULL;

	if (record->length == 0)
		return;

	/* Check that this ack can acknowledge at least the current minimum */
	if (ack <= record->array[record->lower_idx].seq)
		return;

	/* Store old lower index for clean up at end */
	old_lower_idx = record->lower_idx;

	/* Look for packet in array */
	while (record->length > 1 && record->array_size > 0) {

		/* Test the next record to see if it can be acknowledged */
		if (ack <= record->array[(record->lower_idx + 1) % record->array_size].seq)
			break;

		/* Acknowledge current record by moving on */
		record->length--;
		record->lower_idx++;
		if (record->lower_idx == record->array_size)
			record->lower_idx = 0;
	}

	packet = &(record->array[record->lower_idx]);

	/* Look for packet in linked list */
	while (packet->missing_link != NULL) {
		/* Test the next record to see if it can be acknowledged */
		if (ack <= packet->missing_link->seq)
			break;

		/* Acknowledge current record by moving on */
		packet = packet->missing_link;
	}

	/* Record acknowledgement */
	packet->num_acks++;

	/* Clean up spares */
	while (old_lower_idx != record->lower_idx) {

		packet_record_free_missing_links (&(record->array[old_lower_idx]));

		record->array[old_lower_idx].missing_link = NULL;

		old_lower_idx++;
		if (old_lower_idx == record->array_size)
			old_lower_idx = 0;
	}
}

/*
 * Finds and returns the packet record given a sequence number.
 */
struct packet_record_t *sender_record_find (struct sender_record_t *record, uint32_t seq) {
	/* Perform a linear search on array as seq likely to be near beginning.
	 * This may be made into a binary search if extra speed is required.
	 */

	/* Two loops are necessary. The first looks in the array for thr highest
	 * sequence number less than the current sequence number. The next loop
	 * processed the missing links array to look for the same.
	 */

	int idx = record->lower_idx;

	struct packet_record_t *packet = NULL;

	/* First loop. */
	int counter=0;
	for (counter = 0; counter < record->length; counter++) {

		if (record->array[idx].seq > seq)
			break;

		packet = &(record->array[idx]);

		idx++;
		if (idx == record->array_size)
			idx = 0;
	}

	if (packet == NULL)
		return NULL;

	if (packet->seq == seq)
		return packet;

	/* Second loop. */
	while (packet->missing_link != NULL) {
		/* Go through list */

		if (packet->missing_link->seq > seq)
			break;

		packet = packet->missing_link;
	}

	return packet;
}

/*
 * Allocates and initialises a new data structure for a new tcp session.
 * Nothing is allocated to the packet record until data starts moving.
 * This is more efficient in both memory and time.
 */
void *reordering_create () {
	struct reordering_t *reordering = malloc (sizeof (struct reordering_t));
	int i;
	for (i = 0; i < 2; i++) {
		reordering->record[i].lower_idx = 0;
		reordering->record[i].length = 0;
		reordering->record[i].array_size = 0;
		reordering->record[i].array = NULL;
		reordering->record[i].expected_seq = 0;
		reordering->record[i].in_recovery = 0;		
	}

	reordering->rtt_data = rtt_module->session_module.create ();

	reordering->min_rtt=-1.0;
	
	return reordering;
}

/*
 * Frees the data structure of a closed tcp session.
 */
void reordering_destroy (void *data) {
	struct reordering_t *reordering = (struct reordering_t *) data;
	int i;

	/* Free RTT first */
	rtt_module->session_module.destroy (reordering->rtt_data);
	reordering->rtt_data = NULL;

	/* Free missing links */
	for (i = 0; i < reordering->record[0].array_size; i++) {
		packet_record_free_missing_links (&(reordering->record[0].array[i]));
	}
	for (i = 0; i < reordering->record[1].array_size; i++) {
		packet_record_free_missing_links (&(reordering->record[1].array[i]));
	}

	free (reordering->record[0].array);
	free (reordering->record[1].array);
	free (data);
}

/*
 * Updates the the reordering given a new packet belonging to the flow.
 */
void reordering_update (void *data, struct libtrace_packet_t *packet) {
	struct reordering_t *reordering = (struct reordering_t *) data;
	double time_lag, rtt, rto, inside_rtt, outside_rtt;
	struct packet_record_t *packet_record, *prev_packet_record, *next_packet_record;
	int payload;
	uint32_t seq;
	uint16_t ip_id;
	struct libtrace_ip *ip = NULL;
	struct libtrace_tcp *tcp = NULL;
	int direction;
	double time;
	struct sender_record_t *record = NULL;

	ip = trace_get_ip ((struct libtrace_packet_t *)packet);
	tcp = trace_get_tcp ((struct libtrace_packet_t *)packet);
	direction = trace_get_direction (packet);
	time = trace_get_seconds (packet);
	payload = ntohs (ip->ip_len) - ((ip->ip_hl + tcp->doff) << 2);
	seq = ntohl (tcp->seq);
	ip_id = ntohs (ip->ip_id);
	
	if (direction < 0 || direction > 1)
		return;

	record = &(reordering->record[direction]);

	/* Update RTT first */
	rtt_module->session_module.update (reordering->rtt_data, packet);

	/* Get RTT and RTO */
	rtt = -1.0;
	rto = -1.0;
	inside_rtt = rtt_module->inside_rtt (reordering->rtt_data);
	outside_rtt = rtt_module->outside_rtt (reordering->rtt_data);
	if ((inside_rtt >= 0.0) && (outside_rtt >= 0.0)) {
		rtt = inside_rtt + outside_rtt;
		rto = RTO_FACTOR * rtt;
		/* Update minimum RTT */
		if((reordering->min_rtt < rtt) || (reordering->min_rtt < 0.0)) {
			reordering->min_rtt=rtt;
		}
		rtt = RTT_FACTOR*reordering->min_rtt;
	}

	/* Assume current packet is in order */
	reordering->last_packet = INORDER;
	reordering->last_packet_message = 0;
	reordering->time_lag = 0.0;

	/* If packet is a SYN, then set the  ACK */
	if (tcp->syn) {
		record->expected_seq = seq + 1;
		return;
	}
	/* Check if it's a data packet */
	if (payload > 0) {

		/* Check expected_seq against actual */
		if (seq > record->expected_seq) {
			/*printf ("Too high\n"); */
			reordering->last_packet = HIGH;
			reordering->last_packet_message = 1;
			reordering->time_lag = 0.0;

			/* Make two records, one corresponding to the missing packet
			 * the other corresponding to the packet seen
			 */

			/* First record */
			packet_record = sender_record_add (record, record->expected_seq, time, 0);

			packet_record->is_missing = 1;

			/* Second record */
			sender_record_add (record, seq, time, ip_id);

			/* Change expected_seq */
			record->expected_seq = seq + payload;

			/* Once a new packet is received, the sender is not in recovery mode */
			record->in_recovery = 0;

		} else if (seq == record->expected_seq) {
			/* printf ("OK\n"); */
			reordering->last_packet = INORDER;
			reordering->last_packet_message = 0;

			/* Once a new packet is received, the sender is not in recovery mode */
			record->in_recovery = 0;

			/* Add details to array */
			sender_record_add (record, seq, time, ip_id);

			/* Change expected_seq */
			record->expected_seq += payload;
		} else {
			/*printf ("Too low\n"); */

			packet_record = sender_record_find (record, seq);

			if (packet_record == NULL) {
				/*printf ("OO: unneeded retransmission (not found. Data: expected=%8x observed=%8x minimum=%8x)\n", record->expected_seq, seq, record->array[record->lower_idx].seq); */
				reordering->last_packet = RETRANSMISSION;
				reordering->last_packet_message = 2;
				reordering->time_lag = 0.0;
			} else {

				/* Find time lag */
				time_lag = time - packet_record->time;
				reordering->time_lag = time_lag;

				if (packet_record->num_acks > 0 && packet_record->seq == seq) {
					/*printf ("OO: unneeded retransmission (already acked)\n"); */
					reordering->last_packet = RETRANSMISSION;
					reordering->last_packet_message = 3;
				} else {
					prev_packet_record = sender_record_find (record, seq - 1);
					if (prev_packet_record == NULL) {
						/* printf ("OO: error cannot find dup acks\n"); */
						reordering->last_packet = RETRANSMISSION;
						reordering->last_packet_message = 4;
					} else {
						int dup_acks = prev_packet_record->num_acks;

						/* printf("Time lag=%.6f \t RTT=%.6f \t RTO=%.6f\n", time_lag, rtt, rto); */

						if (packet_record->is_missing == 0) {
							/* Packet already seen */

							/* Check that the size of the current packet is the same as the original */
							next_packet_record = sender_record_find (record, seq + payload);

							if (next_packet_record != NULL) {
								if (packet_record->seq != seq || next_packet_record->seq != seq + payload) {
									/* printf("OO: misalignment found\n"); */
									packet_record->is_misaligned = 1;
									next_packet_record->is_misaligned = 1;
								}
							}

							if (ip_id != packet_record->ip_id) {
								/*printf ("OO: retransmission (IP ID different)\n"); */
								reordering->last_packet = RETRANSMISSION;
								reordering->last_packet_message = 5;
								record->in_recovery = 1;
							} else if ((rto >= 0.0) && (time_lag > rto)) {
								/* printf ("OO: retransmission (time_lag > rto)"); */
								reordering->last_packet = RETRANSMISSION;
								reordering->last_packet_message = 6;
								record->in_recovery = 1;
							} else if (dup_acks >= 3) {
								/* printf ("OO: retransmission (duplicate acks >= 3)\n"); */
								reordering->last_packet = RETRANSMISSION;
								reordering->last_packet_message = 7;
								record->in_recovery = 1;
							} else if (record->in_recovery) {
								/* printf ("OO: retransmission (in recovery)\n"); */
								reordering->last_packet = RETRANSMISSION;
								reordering->last_packet_message = 8;
							} else if ((rtt >= 0.0) && (time_lag < rtt)) {
								/* printf ("OO: network duplicate\n"); */
								reordering->last_packet = NETWORK_DUPLICATE;
								reordering->last_packet_message = 9;
							} else {
								/*printf ("OO: unknown\n"); */
								reordering->last_packet = UNKNOWN;
								reordering->last_packet_message = 10;
							}
						} else {
							/* If this packet completes the missing data, then do nothing
							 * Otherwise make an entry in the missing link with the seq
							 * number of the next missing packet expected
							 */
							next_packet_record = sender_record_find (record, seq + payload);
							if (next_packet_record != NULL) {
								if (next_packet_record->seq == seq) {
									/* next 'not found', so create missing link */
									/* printf ("Missing link created for %8x\n", seq + payload); */
									next_packet_record = malloc (sizeof (struct packet_record_t));
									next_packet_record->ip_id = 0;
									next_packet_record->is_missing = 1;
									next_packet_record->missing_link = packet_record->missing_link;
									next_packet_record->num_acks = 0;
									next_packet_record->seq = seq + payload;
									/* Note: the 'time' of the missing link is the time it was first
									 * missed, not the time that it is created.
									 * E.g. recv 10 20 50, both 30 and 40 are missing at the same time
									 * but it is not yet known if the missing packet is size 20
									 */
									next_packet_record->time = packet_record->time;
									packet_record->missing_link = next_packet_record;
								}
							} else {
								/* Pathological case not encountered */
								printf ("Next packet record is NULL\n");
							}

							if (dup_acks >= 3) {
								reordering->last_packet = RETRANSMISSION;
								reordering->last_packet_message = 7;
								record->in_recovery = 1;
							} else if ((rto >= 0.0) && (time_lag > rto)) {
								/* printf ("OO: retransmission (time_lag > rto)"); */
								reordering->last_packet = RETRANSMISSION;
								reordering->last_packet_message = 6;
								record->in_recovery = 1;
							} else if (record->in_recovery) {
								/* printf ("OO: retransmission (in recovery)\n"); */
								reordering->last_packet = RETRANSMISSION;
								reordering->last_packet_message = 8;
							} else if ((rtt >= 0.0) && (time_lag < rtt)) {
								/* printf ("OO: network reordering\n"); */
								reordering->last_packet = NETWORK_REORDERING;
								reordering->last_packet_message = 11;
							} else {
								/* printf ("OO: unknown\n"); */
								reordering->last_packet = UNKNOWN;
								reordering->last_packet_message = 10;
							}
						}
					} /* END prev packet record not null */
				} /* END else already acked */
			} /* END else packet_record is not NULL */
		} /* END else sequence is too low */

	} /* END if (payload > 0) */
	/* Process acknowledgement */
	record = &(reordering->record[1 - direction]);
	sender_record_ack (record, ntohl (tcp->ack_seq));

}

/*
 * This returns the session module for use by the session manager.
 */
struct session_module_t *reordering_module () {
	struct session_module_t *module = malloc (sizeof (struct session_module_t));
	module->create = &reordering_create;
	module->destroy = &reordering_destroy;
	module->update = &reordering_update;
	return module;
}

/*
 * Allows the rtt measurement scheme to be customised.
 */
void reordering_set_rtt_module (struct rtt_module_t *module) {
	rtt_module = module;
}

/*
 * Returns the reordering type of the last packet.
 */
reordering_type_t reordering_get_type (void *data) {
	struct reordering_t *reordering = (struct reordering_t *) data;
	return reordering->last_packet;
}

/*
 * Returns the reason for the order classification of the last packet.
 */
const char *reordering_get_message (void *data) {
	struct reordering_t *reordering = (struct reordering_t *) data;
	return reordering_messages[reordering->last_packet_message];
}

/*
 * Returns the time lag of the last packet. The time lag is defined as
 * follows:
 * for an inorder packet,
 * 		time lag = 0
 * for a duplicate packet, 
 * 		time lag = difference between two viewings
 * for a reordered packet, 
 * 		time lag = difference between expected arrival and actual arrival
 * 							
 * The time lag is what is used in the out-of-order classification.
 */
double reordering_get_time_lag (void *data) {
	struct reordering_t *reordering = (struct reordering_t *) data;
	return reordering->time_lag;
}
			/*

			   |
			   |
			   \_/
			   packet already acked?
			   |
			   | YES   
			   |------>   UNNEEDED
			   NO |       RETRANSMISSION
			   \_/
			   packet already seen?
			   |
			   YES   |  NO
			   |-----------------------|
			   |                       |
			   \_/                     \_/
			   ipid different        time lag > RTO
			   OR  time lag > RTO    OR  (dup acks > 3 AND time lag > RTT) 
			   OR  dup acks > 3                 |
			   |                                |
			   | YES                         YES|
			   |-----> TCP RETRANSMISSION <----|
			   NO|                              |NO
			   |                                |
			   \_/                             \_/
			   in fast recovery            in fast recovery
			   AND seq_no < snd high       AND seq_no < snd high
			   |                                |
			   | YES                         YES|
			   |-----> TCP RETRANSMISSION  <----|
			   NO|                              |NO
			   |                                |
			   \_/                             \_/
			   time lag < rtt           time lag < rtt
			   |                                |
			   |                                |
			   \_/                             \_/
			   NETWORK DUPLICATE        NETWORK REORDERING


			   * */
