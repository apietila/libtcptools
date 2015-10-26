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
 * $Id: queue.h 60 2009-05-13 03:59:37Z salcock $
 *
 */



/*
 * A queue is a general purpose data structure. Elements can only be added
 * to one end and removed from the other, and the elements may be iterated
 * over. The purpose of creating this queue is to avoid repeated use of the
 * malloc() command and this is achieved by using an array with index pointers
 * and allowing additions to wrap around to the beginning.
 */

typedef struct queue_t queue_t;

/*
 * The queue_vars struct holds the buffer size of the queue and how much the
 * buffer can be incremented by. An increment of -1 allows the queue to grow
 * indefinitely. The item size gives the size of the queue elements
 */
struct queue_vars_t {
	int buffer_size;
	int buffer_increment;
	unsigned int item_size;
};

/*
 * A queue iterator
 */
struct queue_itr_t {
	unsigned short int count;
	unsigned int pos;
};

/*
 * Allocates a new queue.
 */
struct queue_t *queue_create ();

/*
 * Frees the memory associated with a queue.
 */
void queue_destroy (struct queue_t *queue);

/*
 * Adds an element to the queue, and returns it. Note: the memory is copied
 * from the pointer "item" to the local data structure.
 */
void *queue_add (struct queue_t *queue, struct queue_vars_t *vars, void *item);

/*
 * Removes an element from the queue and returns it.
 */
void *queue_remove (struct queue_t *queue, struct queue_vars_t *vars);

/*
 * Removes all elements from the queue.
 */
void queue_clear (struct queue_t *queue);

/*
 * Returns the lowest element of the queue.
 */
void *queue_bottom (struct queue_t *queue, struct queue_vars_t *vars);

/*
 * Returns the highest element of the queue.
 */
void *queue_top (struct queue_t *queue, struct queue_vars_t *vars);

/*
 * Initialises an iterator to point to the first element of the queue.
 */
void *queue_itr_begin (struct queue_t *queue, struct queue_vars_t *vars, struct queue_itr_t *itr);

/*
 * Gets the next element of the queue.
 */
void *queue_itr_next (struct queue_t *queue, struct queue_vars_t *vars, struct queue_itr_t *itr);

/*
 * Removes the lowest element from the queue and keeps the iterator valid.
 */
void queue_itr_remove (struct queue_t *queue, struct queue_itr_t *itr);
