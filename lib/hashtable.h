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
 * $Id: hashtable.h 60 2009-05-13 03:59:37Z salcock $
 *
 */


#ifndef HASHTABLE_H_
#define HASHTABLE_H_

typedef struct hashtable_t hashtable_t;
typedef struct hashtable_iterator_t hashtable_iterator_t;

/*
 * Creates and initialises a new hashtable
 */
hashtable_t *hashtable_create ();

/*
 * Frees the memory associated with a hashtable
 */
void hashtable_destroy (hashtable_t * hashtable);

/*
 * Inserts a session into the hashtable
 */
void hashtable_insert (hashtable_t * hashtable, tcp_session_t * session);

/*
 * Retrieves a session from the hashtable givens its ID, or returns 
 * NULL if the session does not exist in the hashtable.
 */
tcp_session_t *hashtable_retrieve (hashtable_t * hashtable, tcp_session_id_t * id);

/*
 * Removes and returns a session from the hashtable given its ID.
 */
tcp_session_t *hashtable_remove (hashtable_t * hashtable, tcp_session_id_t * id);

/*
 * Creates and initialises a new iterator over the hashtable. The ordering of
 * the elements is by the hash function and therefore is not based on the
 * order of insertion.
 */
hashtable_iterator_t *hashtable_iterator_create (hashtable_t * hashtable);

/*
 * Gets the next session.
 */
tcp_session_t *hashtable_iterator_next (hashtable_t * hashtable, hashtable_iterator_t * iterator);

/*
 * Removes the session that is currently pointed to by the iterator. Calling
 * the hashtable_remove function while using the iterator will result in
 * a crash.
 */
tcp_session_t *hashtable_iterator_remove (hashtable_iterator_t * iterator);

#endif							/*HASHTABLE_H_ */
