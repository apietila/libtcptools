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
 * $Id: tcpsession.c 60 2009-05-13 03:59:37Z salcock $
 *
 */

#include <libtrace.h>
#include <stdio.h>
#include "tcpsession.h"

void * tcp_session_get_ptr (tcp_session_t * session, int module_id) {
	return session->data[module_id];
}

/*
 * For debugging or otherwise, this allows a state to be printed
 */
const char *tcp_states_text[] = { "SYN_RCVD", "SYN_SENT", "ESTABLISHED", "FIN_WAIT_1", "FIN_WAIT_2",
	"CLOSING", "TIME_WAIT", "CLOSE_WAIT", "LAST_ACK", "CLOSED",
	"RESET"
};

/*
 * For debugging or otherwise, this prints the ID. Note: hexadecimal is
 * used for the IP address as the dotted-decimal is meaningless with
 * the scrambled IP addresses.
 * */
void tcp_session_id_print (tcp_session_id_t * id) {
	printf ("(%8x:%5u , %8x:%5u)", id->ip_a, id->port_a, id->ip_b, id->port_b);
}

/*
 * This string avoids the need for a malloc() every time a string
 * representation of the id is required.
 * */
char tcp_session_id_string_array[40];

/*
 * For debugging or otherwise, this returns a string representation of
 * the ID, for possible inclusion in another printf() with other data.
 * Note: hexadecimal is used for the IP address as the dotted-decimal 
 * is meaningless with the scrambled IP addresses.
 * */
char *tcp_session_id_string (tcp_session_id_t * id) {
	sprintf (tcp_session_id_string_array, "(%8x:%5u , %8x:%5u)", id->ip_a, id->port_a, id->ip_b, id->port_b);
	return tcp_session_id_string_array;
}

/*
 * This is used by the hashtable and session manager as a convenient
 * way to compare two IDs.
 * */
int tcp_session_id_equals (tcp_session_id_t * id1, tcp_session_id_t * id2) {
	if (id1->ip_a != id2->ip_a)
		return 0;
	if (id1->ip_b != id2->ip_b)
		return 0;
	if (id1->port_a != id2->port_a)
		return 0;
	if (id1->port_b != id2->port_b)
		return 0;
	return 1;
}
