/*****************************************************************************
 * Copyright 2018 Bryan Hawkins <spcnvdrr@protonmail.com>                    *
 *                                                                           *
 * Redistribution and use in source and binary forms, with or without        *
 * modification, are permitted provided that the following conditions        *
 * are met:                                                                  *
 *                                                                           *
 * 1. Redistributions of source code must retain the above copyright notice, *
 * this list of conditions and the following disclaimer.                     *
 *                                                                           *
 * 2. Redistributions in binary form must reproduce the above copyright      *
 * notice, this list of conditions and the following disclaimer in the       *
 * documentation and/or other materials provided with the distribution.      *
 *                                                                           *
 * 3. Neither the name of the copyright holder nor the names of its          *
 * contributors may be used to endorse or promote products derived from      *
 * this software without specific prior written permission.                  *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     *
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      *
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    *
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  *
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    *
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    *
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      *
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        *
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              *
 *                                                                           *
 * This file contains the function declarations for the proc_list linked     *
 * list. This is just a simple linked list that is used to keep track of     *
 * TTYs while wiping the u/w/btmp log files. This is needed because          *
 * when a user logs out, wtmp copies that user's login entry and writes      *
 * it back to the wtmp file except the type is changed to DEAD_PROCESS       *
 * and the user name field in the struct is NUL'ed out/ empty. Since we      *
 * cannot find the user's logout entries by username, we must use            *
 * something else, hence keeping track of the user's tty.                    *
 * Below is an example of what wtmp does when a user named Fred logs in      *
 * and out on tty1:                                                          *
 * type: USER_PROCESS  user: Fred    tty: tty1 time: 03/06/2018 09:05:19     *
 * type: DEAD_PROCESS  user:         tty: tty1 time: 03/06/2018 09:05:40     *
 * type: LOGIN_PROCESS user: LOGIN   tty: tty1 time: 03/06/2018 09:05:40     *
 *                                                                           *
 * Note: Generally, utmp structures have more fields than what is shown      *
 * in the small snippet above (pid, host, etc.), but it has been             *
 * condensed for readability                                                 *
 *****************************************************************************/
#include <sys/types.h>

#ifndef PROC_LIST_H_
#define PROC_LIST_H_

/* Max length of string to store tty */
#define MAXTTY	50

struct tty_list{
	char *tty_line;	                /* A string to hold ut_line */
	struct tty_list *tty_next;    /* Pointer to next member in linked list */
};


/** Create, initialize, and link in a new node with the given properties
 * @param head a double pointer to the first node of the linked list
 * @param tty the device name/tty to create node with, from ut_line
 *
 */
void create_node(struct tty_list **head, char *tty);


/** Find a node member with the given TTY value
 * @param head a pointer to the start of the list
 * @param tty the device name/tty of the node to find
 * @returns a pointer to the node or NULL if not found
 *
 */
struct tty_list *find_tty(struct tty_list *head, char *tty);


/** Unlink and free a node from the list specified by the tty.
 * @param head a double pointer to the first node of the linked list
 * @param tty the device name of the node to remove
 *
 */
void delete_tty(struct tty_list **head, char *tty);


/** Free the entire list. Note that once a list is
 * freed, the head pointer is set to NULL.
 * @param head a double pointer to the head of the list
 *
 */
void free_list(struct tty_list **head);

#endif /* PROC_LIST_H_ */
