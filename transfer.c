#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>

#include "transfer.h"

void init_transfer_list(transfer_list_t* list) {
	*list = NULL;
}

int add(transfer_list_t* list, struct transfer* new_transfer) {
	struct transfer* it;
	if (list == NULL) return -1;
	if (*list == NULL) {
		*list = new_transfer;
		new_transfer->next = NULL;
		return 0;
	}
	for(it=*list; it->next; it=it->next); // Scorro fino all'ultimo
	it->next = new_transfer;
	new_transfer->next = NULL;
	return 0;
}

void deallocate(struct transfer* transfer) {
	if (!transfer) return;
	if (transfer->addr)
		free(transfer->addr);
	if (transfer->filepath)
		free(transfer->filepath);
	free(transfer);
}

void remove_transfer(transfer_list_t* list, struct transfer* target) {
	struct transfer* it,* prev;
	if(*list == NULL) return;
	if(*list == target) {
		*list = (*list)->next;
		deallocate(target);
		return;
	}
	for(it = *list; it && it!=target; it = it->next)
		prev = it;
	if(it == NULL) return;
	prev->next = it->next;
	deallocate(it);
}

struct transfer* create_transfer(int id, struct sockaddr* addr, char* filepath, int mode) {
	struct transfer* new_transfer = malloc(sizeof(struct transfer));
	new_transfer->id = id;
	new_transfer->addr = malloc(sizeof(struct sockaddr));
	memcpy(new_transfer->addr, addr, sizeof(struct sockaddr));
	pthread_cond_init(&new_transfer->acked, NULL);
	pthread_mutex_init(&new_transfer->mutex, NULL);
	new_transfer->filepath = malloc(strlen(filepath) + 1);
	strcpy(new_transfer->filepath, filepath);
	new_transfer->filemode = mode;
	new_transfer->next = NULL;
	return new_transfer;
}

void print_transfer_list(transfer_list_t list) {
	char paddr[INET_ADDRSTRLEN];
	struct sockaddr_in* addrp;
	for(; list; list=list->next) {
		printf("{\n");
		printf("\tId: %d\n", list->id);
		addrp = (struct sockaddr_in*) list->addr;
		inet_ntop(AF_INET, &addrp->sin_addr, paddr, INET_ADDRSTRLEN);
		printf("\tAddr: %s:%d\n", paddr, ntohs(addrp->sin_port));
		printf("\tFilepath: %s\n", list->filepath);
		printf("\tFilemode: %d\n", list->filemode);
		printf("}\n");
	}
}