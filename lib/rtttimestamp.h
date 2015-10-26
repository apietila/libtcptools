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
 * $Id: rtttimestamp.h 60 2009-05-13 03:59:37Z salcock $
 *
 */


#ifndef RTTTIMESTAMP_H_
#define RTTTIMESTAMP_H_

/*
 * This returns the session module for use by the session manager.
 */
struct session_module_t *rtt_timestamp_module ();

/*
 * This returns the rtt module for use by the reordering module.
 */
struct rtt_module_t *rtt_timestamp_rtt_module ();

/*
 * Return the total RTT.
 */
double rtt_timestamp_total (void *data);

/*
 * Return the RTT for the inside half of the connection.
 */
double rtt_timestamp_inside (void *data);

/*
 * Return the RTT for the outside half of the connection.
 */
double rtt_timestamp_outside (void *data);

/*
 * Return the average RTT over the duration of the session
 */
double rtt_timestamp_average (void *data);

#endif							/*RTTTIMESTAMP_H_ */
