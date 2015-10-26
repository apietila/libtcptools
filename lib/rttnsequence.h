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
 * $Id: rttnsequence.h 60 2009-05-13 03:59:37Z salcock $
 *
 */


#ifndef RTTNSEQUENCE_H_
#define RTTNSEQUENCE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The buffer size corresponds to how many unacknowledged packets we can
 * remember at one time. A value of -1 is used to specify that there is no
 * limit on the buffer size and it can grow to accommodate the packets.
 */
void rtt_n_sequence_set_buffer_size (int size);

/*
 * Return the total RTT.
 */
double rtt_n_sequence_total (void *data);

/*
 * Return the RTT for the inside half of the connection.
 */
double rtt_n_sequence_inside (void *data);

/*
 * Return the RTT for the outside half of the connection.
 */
double rtt_n_sequence_outside (void *data);

/*
 * Return the average RTT over the duration of the session
 */
double rtt_n_sequence_average (void *data);

/*
 * This returns the session module for use by the session manager.
 */
struct session_module_t *rtt_n_sequence_module ();

/*
 * This returns the rtt module for use by the reordering module.
 */
struct rtt_module_t *rtt_n_sequence_rtt_module ();

double rtt_n_sequence_variation (void *data);

#ifdef __cplusplus
}
#endif


#endif							/*RTTNSEQUENCE_H_ */
