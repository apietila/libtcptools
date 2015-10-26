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
 * $Id: queue.c 62 2010-02-23 02:11:03Z salcock $
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>
#include <assert.h>

#include "queue.h"

/*
 * This struct has the internals of the queue. Note that ptr is an array
 * on top of which the queue sits. The queue is specified as the pair of
 * integers lower_idx and length.
 */
struct queue_t {
	/*
	 * The data contained in this queue.
	 */
	char *ptr;

	/*
	 * The number of data items in the queue.
	 */
	uint32_t length;

	/*
	 * The position of the lowest data item in the queue.
	 */
	uint32_t lower_idx;

	/*
	 * The number of data items that ptr can hold.
	 */
	uint32_t buffer_size;
};

/*
 * Allocates a new queue.
 */
struct queue_t *queue_create () {
	struct queue_t *queue = malloc (sizeof (struct queue_t));

	queue->buffer_size = 0;
	queue->ptr = NULL;
	queue->length = 0;
	queue->lower_idx = 0;

	return queue;
}

/*
 * Frees the memory associated with a queue.
 */
void queue_destroy (struct queue_t *array) {
	free (array->ptr);
	array->ptr = NULL;
	free (array);
}

/*
 * Adds an element to the queue, and returns it. Note: the memory is copied
 * from the pointer "item" to the local data structure.
 */
void *queue_add (struct queue_t *array, struct queue_vars_t *vars, void *item) {

	int idx;

	/* See if there is space to put item in */
	if (array->buffer_size == 0) {
		/* Buffer hasn't yet been created */
		if (vars->buffer_size == -1) {
			array->buffer_size = vars->buffer_increment;
		} else {
			array->buffer_size = vars->buffer_size;
		}
		array->ptr = malloc (array->buffer_size * vars->item_size);
	} else if (vars->buffer_size == -1) {	/* We have an expanding queue */
		if (array->length == array->buffer_size) {
			char *new_ptr;
			int bytes1;
			int bytes2;

			/* Allocate more space */
			array->buffer_size += vars->buffer_increment;
			new_ptr = malloc (array->buffer_size * vars->item_size);

			assert(array->lower_idx <= array->length);
			assert(array->length <= array->buffer_size);

			/* Copy old ptr to new one so that new one starts at zero */
			bytes1 = vars->item_size * (array->length - array->lower_idx);
			bytes2 = vars->item_size * array->lower_idx;
			memcpy (new_ptr, &(array->ptr[bytes2]), bytes1);
			memcpy (&(new_ptr[bytes1]), array->ptr, bytes2);

			/* Free old memory */
			free (array->ptr);
			array->ptr = new_ptr;
			array->lower_idx = 0;
		}
	} else {
		/* This queue cannot grow when full, so return NULL */
		if (array->length == array->buffer_size) {
			return NULL;
		}
	}

	/* Get address to put item */
	idx = (array->lower_idx + array->length) % array->buffer_size;
	idx *= vars->item_size;

	/* Copy to local data structure */
	memcpy (&(array->ptr[idx]), item, vars->item_size);

	array->length++;

	return &(array->ptr[idx]);
}

/*
 * Removes an element from the queue and returns it.
 */
void *queue_remove (struct queue_t *array, struct queue_vars_t *vars) {

	int idx;
	/* Note: remove only from bottom */

	if (array->length == 0)
		return NULL;

	/* Get address */
	idx = array->lower_idx * vars->item_size;

	/* Increment lower idx */
	array->lower_idx++;
	if (array->lower_idx == array->buffer_size)
		array->lower_idx = 0;

	array->length--;

	return &(array->ptr[idx]);
}

/*
 * Removes all elements from the queue.
 */
void queue_clear (struct queue_t *array) {
	array->length = 0;
	array->lower_idx = 0;
}

/*
 * Returns the lowest element of the queue.
 */
void *queue_bottom (struct queue_t *array, struct queue_vars_t *vars) {

	int idx;

	if (array->length == 0)
		return NULL;

	/* Get address */
	idx = array->lower_idx * vars->item_size;

	return &(array->ptr[idx]);
}

/*
 * Returns the highest element of the queue.
 */
void *queue_top (struct queue_t *array, struct queue_vars_t *vars) {

	int idx;

	if (array->length == 0)
		return NULL;

	/* Get address */
	idx = (array->lower_idx + array->length - 1) % array->buffer_size;
	idx *= vars->item_size;

	return &(array->ptr[idx]);
}

/*
 * Initialises an iterator to point to the first element of the queue.
 */
void *queue_itr_begin (struct queue_t *array, struct queue_vars_t *vars, struct queue_itr_t *itr) {
	if (array->length == 0)
		return NULL;

	itr->count = 1;
	itr->pos = array->lower_idx * vars->item_size;

	return &(array->ptr[itr->pos]);
}

/*
 * Gets the next element of the queue.
 */
void *queue_itr_next (struct queue_t *array, struct queue_vars_t *vars, struct queue_itr_t *itr) {

	/* If all elements have been accounted for, return NULL. */
	if (itr->count == array->length)
		return NULL;

	itr->pos += vars->item_size;
	if (itr->pos == (array->buffer_size * vars->item_size)) {
		itr->pos = 0;
	}

	itr->count++;

	return &(array->ptr[itr->pos]);
}

/*
 * Removes the lowest element from the queue and keeps the iterator valid.
 */
void queue_itr_remove (struct queue_t *array, struct queue_itr_t *itr) {
	array->lower_idx++;
	if (array->lower_idx == array->buffer_size)
		array->lower_idx = 0;

	array->length--;
	itr->count--;
}
