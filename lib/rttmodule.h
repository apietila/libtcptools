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
 * $Id: rttmodule.h 60 2009-05-13 03:59:37Z salcock $
 *
 */


#ifndef RTTMODULE_H_
#define RTTMODULE_H_

/*
 * This struct is a prototype for all RTT analysis schemes. 
 * It is used by the reordering analysis tool to get the RTT.
 */
struct rtt_module_t {
	/*
	 * The RTT module should behave as a normal session module too.
	 */
	struct session_module_t session_module;

	/*
	 * inside_rtt should return the rtt of the part of the session
	 * that is inside the university's link, or more formally it is
	 * associated with the half of the connection for which a sender's 
	 * packet's trace_get_packet_direction() returns 0.
	 */
	double (*inside_rtt) (void *);

	/*
	 * inside_rtt should return the rtt of the part of the session
	 * that is inside the university's link, or more formally it is
	 * associated with the half of the connection for which a sender's 
	 * packet's trace_get_packet_direction() returns 1.
	 */
	double (*outside_rtt) (void *);
};



#endif							/*RTTMODULE_H_ */
