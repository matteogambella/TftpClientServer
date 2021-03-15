#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#include "tftp.c"
#include "transfer.c"
#include "file_utils.h"

int server_socket;
transfer_list_t active_transfers;

// Invia un pacchetto di errore ad addr con i campi specificati
void send_error(struct sockaddr* addr, int error_num, char* msg) {
	if (addr == NULL || msg == NULL) return;
	char* error_packet;
	int size = ERROR_HEADER_LEN + strlen(msg) + 1;

	error_packet = malloc(size);

	set_opcode(error_packet, ERROR);
	set_errornumber(error_packet, error_num);
	set_errormessage(error_packet, msg);

	sendto(server_socket, error_packet, size, 0, addr, sizeof(struct sockaddr));

	free(error_packet);
}

void* handle_transfer(void* args) {
	struct transfer* new_transfer = (struct transfer*) args;
	char data_packet[MAX_DATA_LEN];
	char file_chunk[CHUNK_SIZE];
	int chunks;
	int filesize;
	int done = 0;
	int next_size = 0;
	char paddr[INET_ADDRSTRLEN];
	struct sockaddr_in* addr;
	pthread_mutex_t tmp_mutex;

	// Mutua esclusione su new_transfer
	pthread_mutex_lock(&new_transfer->mutex);

	addr = (struct sockaddr_in*)new_transfer->addr;
	inet_ntop(AF_INET, &addr->sin_addr, paddr, INET_ADDRSTRLEN); // paddr indirizzo printable

	if ((filesize = get_file_size(new_transfer->filepath)) == -1) {
		printf("Errore file not found.\n");
		send_error(new_transfer->addr, FILE_NOT_FOUND, "File not found");
		goto end_transfer;
	}

	// Calcolo del numero di blocchi
	chunks = (int) (filesize/CHUNK_SIZE + 1);
	// il +1 copre anche il caso di file con dim. multipla di CHUNKSIZE

	printf("Inizio trasferimento di %s (%d bytes) in %d blocchi verso %s:%d.\n",
		   new_transfer->filepath, filesize, chunks, paddr, ntohs(addr->sin_port));

	while(done < chunks) {
		if (done == chunks-1) // Se il prossimo e' l'ultimo blocco
			next_size = filesize - done*CHUNK_SIZE;
		else
			next_size = CHUNK_SIZE;

		// Creo un pacchetto data con il blocco
		set_opcode(data_packet, DATA);
		set_blocknumber(data_packet, done);
		if ((get_file_chunk(file_chunk, new_transfer->filepath,
							done*CHUNK_SIZE, next_size, new_transfer->filemode)) == -1) {
			printf("\nSi è verificato un errore durante la lettura del file %s\n",
					new_transfer->filepath);
			send_error(new_transfer->addr, FILE_NOT_FOUND, "File not found");
			goto end_transfer;
		}
		set_data(data_packet, file_chunk, next_size);

		// Se ho inviato tutto il pacchetto data_packet
		if(sendto(server_socket, data_packet, DATA_HEADER_LEN+next_size, 0,
			   new_transfer->addr, sizeof(struct sockaddr)) == DATA_HEADER_LEN+next_size) {			
			printf("\rInviato blocco %d a %s:%d", done, paddr, ntohs(addr->sin_port));
			done++;
			// Sospendo il thread in attesa che arrivi l'ack appropriato
			pthread_cond_wait(&new_transfer->acked, &new_transfer->mutex);
		}
	}

	printf("\nTrasferimento di %s verso %s:%d completato.\n",
		   new_transfer->filepath, paddr, ntohs(addr->sin_port));

end_transfer:
	tmp_mutex = new_transfer->mutex;
	remove_transfer(&active_transfers, new_transfer);
	pthread_mutex_unlock(&tmp_mutex);

	pthread_exit(NULL);
}

int main(int argc, char** argv) {
	// Variabili
	int id_counter = 0;
	char* directory;
	char* filepath;
	char* filename;
	int port;
	int exit_status;
	int mode;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	struct sockaddr_in server_addr;
	char buffer[MAX_REQ_LEN];
	struct transfer* new_transfer;
	pthread_t thread;
	char paddr[INET_ADDRSTRLEN];

	// Inizializzo la lista degli utenti
	init_transfer_list(&active_transfers);

	// Controllo parametri server
	if(argc < 2) {
		printf("Errore parametri.\nChiamare con ./tftp_server <porta> <directory>\n");
		return 0;
	} else if(argc == 2) {
		directory = argv[1];
		port = 69; // Porta di default
	} else {
		port = atoi(argv[1]);
		directory = argv[2];
	}

	printf("TFTP Directory: %s\n", directory);
	printf("TFTP Port: %d\n", port);

	// Inizializza struttura indirizzo server
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	// Socket UDP del server
	server_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (server_socket == -1) {
		printf("Errore: impossibile ottenere un socket dal sistema\n");
		return 0;
	}

	// Associo l'indirizzo e la porta al socket
	exit_status = bind(server_socket, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
	if (exit_status == -1) {
		printf("Non è stato possibile assegnare la porta %d al server.\n", port);
		printf("Se si utilizza la porta di default (69) eseguire il server da root.\n");
		close(server_socket);
		return 0;
	}

	while(1) {
		// Ricevo un messaggio
		client_addr_len = sizeof(struct sockaddr_in);
		recvfrom(server_socket, buffer, MAX_REQ_LEN, 0,
				 (struct sockaddr*)&client_addr, &client_addr_len);
		if (get_opcode(buffer) == RRQ) { // Se ho ricevuto una richiesta
			// Aggiungo un nuovo utente alla lista
			filename = get_filename(buffer); // e.g. testo.txt
			filepath = malloc(strlen(directory) + strlen(filename) +1);
			strcpy(filepath, directory);
			strcat(filepath, filename); // filepath sarà file_dir/testo.txt

			if (strcmp(get_filemode(buffer), TEXT_MODE) == 0)
				mode = TEXT;
			else
				mode = BIN;

			new_transfer = create_transfer(id_counter++, (struct sockaddr*)&client_addr, 
											filepath, mode);
			
			inet_ntop(AF_INET, &client_addr.sin_addr, paddr, INET_ADDRSTRLEN);
			printf("Ricevuta richiesta di download per %s in modalità %s da %s:%d \n",
				   filepath, get_filemode(buffer), paddr, ntohs(client_addr.sin_port));
			// Libero le stringhe che non servono più
			free(filename);
			free(filepath);

			if(add(&active_transfers, new_transfer)) {
				deallocate(new_transfer);
				id_counter--;
				continue;
			}

			// Avvio un nuovo thread che gestira' l'invio col client
			if(pthread_create(&thread, NULL, handle_transfer, (void*)new_transfer)) {
				deallocate(new_transfer);
				id_counter--;
				continue;
			}
		} else if (get_opcode(buffer) == ACK) { // Ricevo un ack
			// Risveglio il thread che sta gestendo il mittente dell'ack
			new_transfer = get_transfer_byaddr(active_transfers, &client_addr);
			// Transfer non trovato puo' capitare se arriva un ack dopo che
			// il trasferimento si e' chiuso con errore
			if (new_transfer == NULL)
				continue;
			// Mutua esclusione sulla struttura new_transfer
			pthread_mutex_lock(&new_transfer->mutex);			
			pthread_cond_signal(&new_transfer->acked); // Risveglio il thread (suppongo un solo thread per transfer)
			pthread_mutex_unlock(&new_transfer->mutex);
		} else { // Messaggio in nessun formato accettabile
			printf("Non gestito pacchetto con opcode %d.\n",
				   get_opcode(buffer));
			send_error((struct sockaddr*)&client_addr, ILLEGAL_TFTP_OP, "Illegal tftp operation");
		}
	}

	return 0;
}