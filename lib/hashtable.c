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
 * $Id: hashtable.c 60 2009-05-13 03:59:37Z salcock $
 *
 */


#define ARRAY_SIZE 2000003

#include <stdlib.h>
#include <stdio.h>
#include <libtrace.h>
#include "sessionmanager.h"
#include "hashtable.h"
#include "tcpsession.h"

/*
 * This struct is the hash table, i.e. an array.
 */
struct hashtable_t {
	struct hash_entry **arr;
};

/*
 * This struct is an entry in the hash table.
 */
struct hash_entry {
	/*
	 * The session to which the entry refers
	 */
	tcp_session_t *session;

	/*
	 * A pointer to the next entry hashing to the same position
	 * in the array. This is the external chaining method of
	 * hash tables.
	 */
	struct hash_entry *next;
};

/*
 * This struct is for the iterator. Using a hashtable with external
 * chaining, both the array index and chain index need to be stored
 * if the remove function is to work correctly.
 * */
struct hashtable_iterator_t {
	unsigned int array_position;
	unsigned int list_position;
	struct hash_entry **entry_ptr;
};

/*
 * Computes the hash of a tcp session id
 * */
int hashtable_compute_hash (tcp_session_id_t * id);

/*
 * Creates and initialises a new hashtable
 */
hashtable_t *hashtable_create () {
	int i;
	hashtable_t *hashtable = (hashtable_t *) malloc (sizeof (hashtable_t));
	hashtable->arr = (struct hash_entry **) malloc (ARRAY_SIZE * sizeof (struct hash_entry *));
	for (i = 0; i < ARRAY_SIZE; i++)
		hashtable->arr[i] = NULL;
	return hashtable;
}

/*
 * Frees the memory associated with a hashtable
 */
void hashtable_destroy (hashtable_t * hashtable) {
	int i;
	struct hash_entry *ptr;
	for (i = 0; i < ARRAY_SIZE; i++) {
		while (hashtable->arr[i] != NULL) {
			ptr = hashtable->arr[i]->next;
			free (hashtable->arr[i]->session);
			free (hashtable->arr[i]);
			hashtable->arr[i] = ptr;
		}
	}
	free (hashtable->arr);
	free (hashtable);
}

/* Computes the hash of a flow's IP addresses and TCP ports */
int hashtable_compute_hash (tcp_session_id_t * id) {
	unsigned long int key;

	key = (1 + id->ip_a) ^ (2 + id->ip_b) ^ (4 + id->port_a) ^ (8 + id->port_b);

	return key % ARRAY_SIZE;
}

/*
 * Inserts a session into the hashtable
 */
void hashtable_insert (hashtable_t * hashtable, tcp_session_t * session) {

	int hash = hashtable_compute_hash (&(session->id));

	struct hash_entry *new_hash_entry = (struct hash_entry *) malloc (sizeof (struct hash_entry));
	new_hash_entry->session = session;

	/* Append new entry to the front of the list as it is more likely
	 * to be accessed than older and possible stale entries
	 */
	new_hash_entry->next = hashtable->arr[hash];
	hashtable->arr[hash] = new_hash_entry;
}

/*
 * Retrieves a session from the hashtable givens its ID, or returns 
 * NULL if the session does not exist in the hashtable.
 */
tcp_session_t *hashtable_retrieve (hashtable_t * hashtable, tcp_session_id_t * id) {

	int hash = hashtable_compute_hash (id);

	struct hash_entry *entry = hashtable->arr[hash];

	/* Iterate through the chain */
	while (entry != NULL) {
		if (tcp_session_id_equals (id, &(entry->session->id)))
			return entry->session;
		else
			entry = entry->next;
	}

	return NULL;
}

/*
 * Removes and returns a session from the hashtable given its ID.
 */
tcp_session_t *hashtable_remove (hashtable_t * hashtable, tcp_session_id_t * id) {

	int hash = hashtable_compute_hash (id);
	struct hash_entry **entry_ptr = &(hashtable->arr[hash]);

	struct hash_entry *entry = hashtable->arr[hash];

	tcp_session_t *session;

	/* It is important to update the chain correctly, and using **entry_ptr
	 * we can do this with minimal fuss, not worry whether or not the
	 * entry is part of the array or part of the chain.
	 */
	while (entry != NULL) {
		if (tcp_session_id_equals (id, &(entry->session->id))) {
			session = entry->session;
			*entry_ptr = entry->next;
			entry->next = NULL;
			entry->session = NULL;
			free (entry);
			return session;
		}
		entry_ptr = &(entry->next);
		entry = entry->next;
	}

	return NULL;
}

/*
 * Creates and initialises a new iterator over the hashtable. The ordering of
 * the elements is by the hash function and therefore is not based on the
 * order of insertion.
 */
hashtable_iterator_t *hashtable_iterator_create (hashtable_t * hashtable) {
	hashtable_iterator_t *iterator = malloc (sizeof (hashtable_iterator_t));
	iterator->array_position = 0;
	iterator->entry_ptr = &(hashtable->arr[0]);
	return iterator;
}

/*
 * Gets the next session.
 */
tcp_session_t *hashtable_iterator_next (hashtable_t * hashtable, hashtable_iterator_t * iterator) {

	if (*(iterator->entry_ptr) == NULL) {

		/* Scan vertically through array */
		do {
			iterator->array_position++;
			/* Check if end of array has been reached */
			if (iterator->array_position == ARRAY_SIZE)
				return NULL;
		} while (hashtable->arr[iterator->array_position] == NULL);
		/* Return the first non-NULL entry found */
		iterator->entry_ptr = &(hashtable->arr[iterator->array_position]);

	} else {

		/* If we already have an entry, try to scan horizontally
		 * through list
		 */
		if ((*(iterator->entry_ptr))->next != NULL) {
			iterator->entry_ptr = &((*(iterator->entry_ptr))->next);
		} else {
			/* End of list reached, so scan vertically through array
			 */
			do {
				iterator->array_position++;
				/* Check if end of array has been reached */
				if (iterator->array_position == ARRAY_SIZE)
					return NULL;
			} while (hashtable->arr[iterator->array_position] == NULL);
			/* Return the first non-NULL entry found */
			iterator->entry_ptr = &(hashtable->arr[iterator->array_position]);
		}

	}

	/* entry_ptr is now valid */
	return (*(iterator->entry_ptr))->session;

}

/*
 * Removes the session that is currently pointed to by the iterator. Calling
 * the hashtable_remove function while using the iterator will result in
 * a crash.
 */
tcp_session_t *hashtable_iterator_remove (hashtable_iterator_t * iterator) {

	struct hash_entry *entry;
	struct tcp_session_t *session;

	if (iterator->entry_ptr == NULL)
		return NULL;

	entry = (*(iterator->entry_ptr));
	session = entry->session;

	if (entry == NULL)
		return NULL;

	/* All-in-one adjustment of hashtable: */
	*(iterator->entry_ptr) = entry->next;
	entry->session = NULL;
	entry->next = NULL;
	free (entry);
	return session;

}
