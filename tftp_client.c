#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tftp.c"
#include "file_utils.c"


#define HELP "!help"
#define MODE "!mode"
#define GET  "!get"
#define QUIT "!quit"
#define MAX_CMD_LINE_LEN 64
#define WRONG_CMD "Comando non trovato. Usare !help per l'elenco dei comandi.\n'"


void open_menu(){

  printf("\n\nSono disponibili i seguenti comandi:\n");
  printf("!help-->mostra l'elenco dei comandi disponibili\n");
  printf("!mode{txt|bin}-->imposta il modo di trasferimento dei files\n");
  printf("!get filename nome_locale--> richiede al server il nome del file <filename> e lo salva localmente con il nome <nome_locale>\n");
  printf("!quit-->termina il client\n");

}

int main(int argc, char* argv[]){

  char* server_ip;
  int   server_port;
  char  command[MAX_CMD_LINE_LEN];
  char* filemode;
  char* filename;
  char buffer[MAX_ERROR_LEN];
  int mode;
  int filename_len;
  char req_packet[MAX_REQ_LEN];
  char* cmd;

  int block;
  int received_block;
  int received;
  int error_number;
  char* data;
  
  int ret,sock;
  struct sockaddr_in srv_addr;

  open_menu();

  // Controllo parametri client

  if(argc<2) {
    printf("Errore parametri. \nChiamare con ./tftp_client <IP server> <porta server>\n");
    return 0;
  } else if(argc==2){
    server_ip=argv[1];
    server_port=69; //Porta di default
  } else {
    server_port=atoi(argv[2]);
    server_ip=argv[1];
  }

  printf("Indirizzo Server: %s\n",server_ip);
  printf("Porta Server: %d\n",server_port);

  // filemode a BIN di default
  filemode=malloc(10);
  strcpy(filemode,BIN_MODE);
  mode=BIN;

    
   // inizializzazione indirizzo server
   
   memset(&srv_addr,0,sizeof(srv_addr)); //Pulizia
   srv_addr.sin_family=AF_INET;
   srv_addr.sin_port=htons(server_port);
   inet_pton(AF_INET,server_ip,&srv_addr.sin_addr);

   //Creazione socket UDP

   sock=socket(AF_INET,SOCK_DGRAM,0);

   if(sock==-1){
     printf("Errore: nessun socket disponibile\n");
     return 0;
   }

   //Associo alla porta UDP l'indirizzo del server
   ret=connect(sock,(struct sockaddr*)&srv_addr,sizeof(srv_addr));

   if(ret==-1){
     printf("Errore di connessione");
     close(sock);
   }


while(1){

    printf("> ");
    fgets(command,MAX_CMD_LINE_LEN,stdin); // prendo l'intera riga';
    // Rimuovo il carattere \n finale
    command[strlen(command)-1]= '\0';

    if(strlen(command)==0){  // stringa vuota
      printf(WRONG_CMD);
      continue;
    }

    cmd=strtok(command," ");    // restituisce puntatore al primo token

    if(strcmp(cmd,HELP)==0){
      open_menu();
    } else if (strcmp(cmd,MODE)==0){
      cmd=strtok(NULL," ");  // prelevo il primo argomento
      if(cmd == NULL) {
        printf("Formato di mode non valido. L'opportuna sintassi è !mode{txt|bin}\n");
        continue;
      }
      if(strcmp(cmd,"bin")==0){
       printf("Modo di trasferimento binario configurato\n");
       strcpy(filemode,TEXT_MODE);
       mode=TEXT;
      } else if (strcmp(cmd,"txt")==0){
       printf("Modo di trasferimento testo configurato\n"); 
       strcpy(filemode,BIN_MODE);
       mode=BIN;
      } else {
       printf("Formato di mode non valido. L'opportuna sintassi è !mode{txt|bin}\n");
     }
  } else if(strcmp(cmd,GET)==0) {
            set_opcode(req_packet, RRQ); // Inizializzo un pacchetto di richiesta
			cmd = strtok(NULL, " "); // Prelevo il filename
			if (cmd == NULL) {
				printf("Formato dell'istruzione get errato\n");
				printf("Sintassi corretta: !get filename nome_locale\n");
				continue;
			}
			filename_len = strlen(cmd); // cmd contiene il filename
			set_filename(req_packet, cmd);
			set_filemode(req_packet, filemode);
			cmd = strtok(NULL, " "); // cmd contiene il nome locale
			if (cmd == NULL) {
				printf("Formato dell'istruzione get errato\n");
				printf("Sintassi corretta: !get filename nome_locale\n");
				continue;
			}
			if ((send(sock, req_packet, REQ_HEADER_LEN+filename_len+1+strlen(filemode)+1,0)
				 ) < REQ_HEADER_LEN+filename_len+1+strlen(filemode)+1) {
				printf("Non è stato possibile inviare la richiesta\n");
				continue;
			}
			filename = get_filename(req_packet); // Prelevo il filename per stamparlo
			printf("Richiesta file %s al server in corso.\n", filename);
			free(filename);

            block = received_block = -1;
			// Ricevo un pacchetto di risposta alla richiesta
			while(1) {
				// La lunghezza del pacchetto che arriva sara' max(MAX_ERROR_LEN, MAX_DATA_LEN)
				received = recv(sock, buffer, MAX_DATA_LEN, 0);
				if (get_opcode(buffer) == ERROR) { // Se ricevo un pacchetto di errore
					error_number = get_errornumber(buffer);
					if (error_number == FILE_NOT_FOUND) {
						printf("File non trovato.\n");
					} else if (error_number == ILLEGAL_TFTP_OP) {
						printf("Operazione tftp non valida.\n");
					}
					break;
				} else if (get_opcode(buffer) == DATA) { // Se ricevo un pacchetto dati
					received_block = get_blocknumber(buffer);
					// Controllo di aver ricevuto il blocco che mi aspettavo
					if (received_block == block + 1)
						block = received_block;
					else
						continue; // Se arriva un blocco che non mi aspetto lo ignoro
					// Se si tratta del primo blocco stampo
					if (block == 0)
						printf("Trasferimento file in corso.\n");
					printf("\rTrasferimento blocco %d", block);
					fflush(stdout); // Forza la stampa precedente che verrebbe bufferizzata
					data = get_data(buffer, received);
					// tok punta ancora al nome locale del file
					append_file_chunk(data,cmd,received-DATA_HEADER_LEN, mode);
					free(data);
					// Preparo il pacchetto di ack
					set_opcode(buffer, ACK);
					set_blocknumber(buffer, block);
					send(sock, buffer, ACK_LEN, 0); // Invio l'ack
					// Se il campo data e' piu' piccolo di un blocco allora era l'ultimo
					if (received-DATA_HEADER_LEN < CHUNK_SIZE) {
						printf("\nTrasferimento completato (%d/%d blocchi)\n", block+1, block+1);
						printf("Salvataggio %s completato\n", cmd);
						break;
					}
				} else { // Se ricevo un pacchetto con opcode non gestito o errato
					printf("Opcode %d errato\n", get_opcode(buffer));
					break;
				}
			}
  } else if(strcmp(cmd,QUIT)==0) {
       if (filemode != NULL) free(filemode);
       break;
  } else {
     printf(WRONG_CMD);
  }
}

close(sock);   
exit(0);
}



  
