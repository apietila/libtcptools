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
 * $Id: sessionmanager.h 61 2010-02-21 21:03:03Z salcock $
 *
 */


#ifndef SESSIONMANAGER_H_
#define SESSIONMANAGER_H_

#include <inttypes.h>
#include <libtrace.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct tcp_session_t tcp_session_t;
typedef struct tcp_session_id_t tcp_session_id_t;

enum tcp_conn_state_t { SYN_RCVD, SYN_SENT, ESTABLISHED, FIN_WAIT_1, FIN_WAIT_2,
        CLOSING, TIME_WAIT, CLOSE_WAIT, LAST_ACK, CLOSED, RESET
};

typedef enum tcp_conn_state_t tcp_conn_state_t;

/*
 * A TCP session is uniquely identifiable by its source and destination
 * IP address and port.
 */
struct tcp_session_id_t {
        uint32_t ip_a;
        uint32_t ip_b;
        uint16_t port_a;
        uint16_t port_b;
};

/*
 * A TCP session consists of an ID, a state and some data associated
 * with the modules which perform analyses on the sessions. Other
 * variables are just for debugging purposes for now.
 */
struct tcp_session_t {
        tcp_session_id_t id;
        tcp_conn_state_t state;
        uint32_t expected_ack;
        uint8_t waiting;
        uint8_t last_access;
        void **data;
};


/*
 * The session module struct is the core component that allows users
 * to specify their own analysis on flows.
 */
struct session_module_t {

        /*
         * The create function is called when a new TCP session is initiated.
         * It should return a pointer to the data structure that the module needs.
         */
        void *(*create) ();

        /*
         * The destroy function is called on a closed, reset or discarded flow.
         * It should free any memory allocated with create, and perhaps call
         * some output functions.
         */
        void (*destroy) (void *);

        /*
         * The update function is called when a new packet identified as belonging
         * to the flow is found. The behaviour of this function will vary from
         * module to module, but it is expected that it will need to use the
         * memory allocated from the create function.
         */
        void (*update) (void *, struct libtrace_packet_t *);

};



typedef struct session_manager_t session_manager_t;

/*
 * Creates and initialises a session manager.
 */
session_manager_t *session_manager_create ();

/*
 * Frees all memory allocated by this session manager.
 */
void session_manager_destroy (session_manager_t * manager);

/*
 * Registers a module with this session manager and returns the index of the
 * module. A tcp_session_t will have an array of pointers, and the index will
 * represent the position in the array of the data associated with the
 * registered module.
 */
int session_manager_register_module (session_manager_t * manager, struct session_module_t *module);

/*
 * This function updates the session to which the packet belongs, and returns
 * the session back to the user. All registerd modules are also updated
 * with the packet.
 */
tcp_session_t *session_manager_update (session_manager_t * manager, struct libtrace_packet_t *packet);

#ifdef __cplusplus
}
#endif


#endif							/*SESSIONMANAGER_H_ */
