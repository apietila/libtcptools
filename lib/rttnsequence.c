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
 * $Id: rttnsequence.c 60 2009-05-13 03:59:37Z salcock $
 *
 */


/*
 * 
 * Sequence numbers stored as their acks.
 * 
 * (Figure out how many sessions there are at a time - use cleanup as this
 * already iterates through).
 * 
 * 20 s rtt timeout
 * 
 * Smooth = (3*old + new) / 4
 */

#include <libtrace.h>
#include <stdlib.h>
#include <stdio.h>
#include "sessionmanager.h"
#include "rttmodule.h"
#include "queue.h"
#include "rttnsequence.h"

#define SMOOTH 0.875
#define VARSMOOTH 0.75

/* This allows our queue of ack/time pairs to grow indefinitely.
 * There is a function which allows the buffer to be set to prevent this.
 */
struct queue_vars_t rtt_n_queue_vars = { -1, 0, 0 };

/*
 * This struct is an item of the queue. We need to store the acks expected
 * along with the time at which the data packet arrived.
 */
struct rtt_n_item_t {
	uint32_t expected_ack;
	double time;
};

/*
 * This struct keeps track of the average rtt over the session and also
 * stores the sequence/time queues for both directions.
 */
struct rtt_n_t {
  /*
   * We need one set of variables for each direction.
   */
  struct {
    
    /*
     * This is an queue of sequence number and time pairs.
     */
    struct queue_t *queue;
    
    /*
     * This is the current rtt estimate for the half connection.
     */
    double rtt;
    
    double rtt_var;
    
    /*
     * These vairables together store the average rtt for the
     * session.
     */
    double total;
    int count;
    
  } dir[2];			/* 0 = outside, 1 = inside */

  /* Last RTT sample or -1.0 if not avail. */
  double last_rtt;
};


/*
 * Allocates and initialises a new data structure for a new tcp session.
 */
void *rtt_n_sequence_create () {
  struct rtt_n_t *rtt_n = malloc (sizeof (struct rtt_n_t));
  int i;
	
  /* Guess that a buffer_increment of 10 will do. */
  rtt_n_queue_vars.buffer_increment = 10;
  rtt_n_queue_vars.item_size = sizeof (struct rtt_n_item_t);

  /* Initialise the variables for both directions. */
  for (i = 0; i < 2; i++) {
    
    rtt_n->dir[i].queue = queue_create (&rtt_n_queue_vars);
    
    rtt_n->dir[i].rtt = -1.0;
    rtt_n->dir[i].rtt_var = -1.0;
    
    rtt_n->dir[i].total = 0.0;
    rtt_n->dir[i].count = 0;
  }
  
  return rtt_n;
}

/*
 * Frees the data structure of a closed tcp session.
 */
void rtt_n_sequence_destroy (void *data) {
  struct rtt_n_t *rtt_n = (struct rtt_n_t *) data;
  queue_destroy (rtt_n->dir[0].queue);
  queue_destroy (rtt_n->dir[1].queue);
  free (rtt_n);
}

/*
 * Updates the RTT estimates given a new packet belonging to the flow.
 */
void rtt_n_sequence_update (void *data, struct libtrace_packet_t *packet) {

  /* Algorithm:
   * 
   * If the packet is a data packet, we want to record the expected sequence
   * number of the ack. If the packet is a retransmit, blank the queue, and 
   * recovery will begin with the next data packet. On each iteration,
   * acknowledge as many packets as possible using the ack sequence number
   * and this generates an RTT estimate, which is then smoothed.
   */
  struct rtt_n_t *rtt_n = (struct rtt_n_t *) data;
  
  struct queue_t *queue;
  struct rtt_n_item_t *item;
  
  struct libtrace_ip *ip = trace_get_ip ((libtrace_packet_t *)packet);
  struct libtrace_tcp *tcp = trace_get_tcp ((libtrace_packet_t *)packet);
  int direction = trace_get_direction (packet);
  double time = trace_get_seconds (packet);
  
  struct queue_itr_t itr;
  uint32_t ack;
  double rtt;
  
  int payload;

  // reset on each packet for this flow, will only have valid
  // value right after a packet update that creates a new sample
  rtt_n->last_rtt = -1.0;

  /* Check that the direction is ok */
  if(!(direction==0 || direction==1))
    return;

  payload = ntohs (ip->ip_len) - ((ip->ip_hl + tcp->doff) << 2);

  /* Only if the packet has data do we record it. */
  if (payload > 0) {
    uint32_t expected = ntohl (tcp->seq) + payload;

    queue = rtt_n->dir[1 - direction].queue;

    item = queue_top (queue, &(rtt_n_queue_vars));

    /* The current packet is a retransmit if the queue is not empty 
     * and 'expected' is not the highest element in the queue.
     */
    if ((item == NULL) || (expected > item->expected_ack)) {
      struct rtt_n_item_t new_item;
      new_item.expected_ack = expected;
      new_item.time = time;
      queue_add (queue, &rtt_n_queue_vars, &new_item);
    } else {
      /* Clear queue so that we start measuring rtt from
       * scratch.
       */
      queue_clear (queue);
    }
    
  }

  queue = rtt_n->dir[direction].queue;

  /* Use the acknowledgement, generating an rtt in the process */
  ack = ntohl (tcp->ack_seq);
  rtt = -1.0;

  /* Iterate through the queue, breaking when we cannot ack any more
   * elements.
   */
  item = queue_itr_begin (queue, &rtt_n_queue_vars, &itr);
  while (item != NULL) {
    if (ack >= item->expected_ack) {
      /* Get estimated RTT and remove acked record. */
      rtt = time - item->time;
      queue_itr_remove (queue, &itr);
    } else {
      break;
    }
    item = queue_itr_next (queue, &rtt_n_queue_vars, &itr);
  }
  
  if (rtt > 0) {
    /* If rtt is too big, do not use it. */
    if (rtt > RTT_N_SEQUENCE_MAX_RTT)
      return;

    rtt_n->last_rtt = rtt;
    rtt_n->dir[direction].total += rtt;
    rtt_n->dir[direction].count++;

    /* Update rtt estimate */
    if (rtt_n->dir[direction].rtt < 0) {
      rtt_n->dir[direction].rtt = rtt;
      rtt_n->dir[direction].rtt_var = rtt / 2;
    } else {				/* Smooth */
      rtt_n->dir[direction].rtt = (SMOOTH * rtt_n->dir[direction].rtt) + ((1 - SMOOTH) * rtt);
      rtt_n->dir[direction].rtt_var = (VARSMOOTH * rtt_n->dir[direction].rtt) + ((1 - VARSMOOTH) * abs(rtt_n->dir[direction].rtt - rtt));
    }
  }
}

/*
 * The buffer size corresponds to how many unacknowledged packets we can
 * remember at one time. A value of -1 is used to specify that there is no
 * limit on the buffer size and it can grow to accommodate the packets.
 */
void rtt_n_sequence_set_buffer_size (int size) {
  if ((size == -1) || ((size > 0) && (size < 65536))) {
    rtt_n_queue_vars.buffer_size = size;
  } else {
    rtt_n_queue_vars.buffer_size = -1;
    fprintf (stderr, "rtt_n_sequence: Buffer size out of range\n");
  }
}

double rtt_n_sequence_variation (void *data) {
  struct rtt_n_t *rtt_n = (struct rtt_n_t *) data;
  if ((rtt_n->dir[0].rtt >= 0) && (rtt_n->dir[1].rtt >= 0))
    return rtt_n->dir[0].rtt_var + rtt_n->dir[1].rtt_var;
  else
    return -1.0;
}

/*
 * Return the total RTT.
 */
double rtt_n_sequence_total (void *data) {
  struct rtt_n_t *rtt_n = (struct rtt_n_t *) data;
  if ((rtt_n->dir[0].rtt >= 0) && (rtt_n->dir[1].rtt >= 0))
    return rtt_n->dir[0].rtt + rtt_n->dir[1].rtt;
  else
    return -1.0;
}

/*
 * Return the last RTT sample.
 */
double rtt_n_sequence_last_sample_value (void *data) {
  struct rtt_n_t *rtt_n = (struct rtt_n_t *) data;
  return rtt_n->last_rtt;
}

/*
 * Return the RTT for the inside half of the connection.
 */
double rtt_n_sequence_inside (void *data) {
  struct rtt_n_t *rtt_n = (struct rtt_n_t *) data;
  if (rtt_n->dir[0].rtt >= 0)
    return rtt_n->dir[0].rtt;
  else
    return -1.0;
}

/*
 * Return the RTT for the outside half of the connection.
 */
double rtt_n_sequence_outside (void *data) {
  struct rtt_n_t *rtt_n = (struct rtt_n_t *) data;
  if (rtt_n->dir[1].rtt >= 0)
    return rtt_n->dir[1].rtt;
  else
    return -1.0;
}

/*
 * Return the average RTT over the duration of the session
 */
double rtt_n_sequence_average (void *data) {
  struct rtt_n_t *rtt_n = (struct rtt_n_t *) data;
  if ((rtt_n->dir[0].total > 0) && (rtt_n->dir[1].total > 0))
    return (rtt_n->dir[0].total / rtt_n->dir[0].count + rtt_n->dir[1].total / rtt_n->dir[1].count);
  else
    return -1.0;
}

/*
 * This returns the session module for use by the session manager.
 */
struct session_module_t *rtt_n_sequence_module () {
  struct session_module_t *module = malloc (sizeof (struct session_module_t));
  module->create = &rtt_n_sequence_create;
  module->destroy = &rtt_n_sequence_destroy;
  module->update = &rtt_n_sequence_update;
  return module;
}

/*
 * This returns the rtt module for use by the reordering module.
 */
struct rtt_module_t *rtt_n_sequence_rtt_module () {
  struct rtt_module_t *module = malloc (sizeof (struct rtt_module_t));
  module->session_module.create = &rtt_n_sequence_create;
  module->session_module.destroy = &rtt_n_sequence_destroy;
  module->session_module.update = &rtt_n_sequence_update;
  module->inside_rtt = &(rtt_n_sequence_inside);
  module->outside_rtt = &(rtt_n_sequence_outside);
  return module;
}
