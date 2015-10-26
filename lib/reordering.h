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
 * $Id: reordering.h 60 2009-05-13 03:59:37Z salcock $
 *
 */


#ifndef REORDERING_H_
#define REORDERING_H_

/*
 * The type of the reordering detected.
 */
enum reordering_type_t { INORDER, HIGH, RETRANSMISSION, NETWORK_REORDERING,
	NETWORK_DUPLICATE, UNKNOWN, LAST_REORDERING };

typedef enum reordering_type_t reordering_type_t;

/*
 * This returns the session module for use by the session manager.
 */
struct session_module_t *reordering_module ();

/*
 * Allows the rtt measurement scheme to be customised.
 */
void reordering_set_rtt_module (struct rtt_module_t *module);

/*
 * Returns the reordering type of the last packet.
 */
reordering_type_t reordering_get_type (void *data);

/*
 * Returns the reason for the order classification of the last packet.
 */
const char *reordering_get_message (void *data);

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
double reordering_get_time_lag (void *data);

#endif							/*REORDERING_H_ */
