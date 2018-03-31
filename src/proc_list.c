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
 * This file contains the function definitions for the linked list used to   *
 * keep track of process IDs. This linked list is meant to be as simple and  *
 * reusable as possible. Nodes are always added to the front/head of the     *
 * list because they are more likely to be searched for and removed first.   *
 *****************************************************************************/
#include "proc_list.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>


/** Create, initialize, and link in a new node with the given properties
 * @param head a double pointer to the first node of the linked list
 * @param tty the device name/tty to create node with, from ut_line
 *
 */
void create_node(struct proc_list **head, char *tty){
	/* Allocate memory for the new node */
	struct proc_list *new = malloc(sizeof(struct proc_list));
	if(new == NULL){
		perror("malloc() error");
		exit(EXIT_FAILURE);
	}

	/* Allocate space to store the ut_line member of utmp structure */
	new->proc_tty = malloc(MAXTTY);
	if(new->proc_tty == NULL){
		perror("malloc() error");
		free(new);
		exit(EXIT_FAILURE);
	}

	strncpy(new->proc_tty, tty, MAXTTY);

	/* Add the new node to the head of the list */
	new->proc_next = (*head);
	(*head) = new;
}


/** Find a node member with the given TTY value
 * @param head a pointer to the start of the list
 * @param tty the device name/tty of the node to find
 * @returns a pointer to the node or NULL if not found
 *
 */
struct proc_list *find_tty(struct proc_list *head, char *tty){
	while(head != NULL){
		if(!(strcmp(head->proc_tty, tty))){
			/* We found the node */
			return(head);
		}
		/* Continue looking... */
		head = head->proc_next;
	}
	/* PID not found in list */
	return(NULL);
}


/** Unlink and free a node from the list specified by the tty.
 * @param head a double pointer to the first node of the linked list
 * @param tty the device name of the node to remove
 *
 */
void delete_tty(struct proc_list **head, char *tty){
	struct proc_list *tmp = NULL;

	while(*head != NULL){
		if(!strcmp((*head)->proc_tty, tty)){
			/* Found the node */
			tmp = (*head);
			/* Remove the node and free it */
			(*head) = (*head)->proc_next;
			free(tmp->proc_tty);
			free(tmp);
			return;
		} else {
			/* Keep looking... */
			head = &((*head)->proc_next);
		}
	}
}


/** Free the entire list. Note that once a list is
 * freed, the head pointer is set to NULL.
 * @param head a double pointer to the head of the list
 *
 */
void free_list(struct proc_list **head){
	struct proc_list *tmp = *head;
	while(*head != NULL){
		*head = (*head)->proc_next;
		free(tmp->proc_tty);
		free(tmp);
		tmp = *head;
	}

	/* Set the now empty list head to NULL */
	*head = NULL;
}
