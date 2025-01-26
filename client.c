#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#define PORT 8080
#define BUFFER_SIZE 1024
#define GRID_SIZE 9 
#define NAME_SIZE 20
#include <sys/select.h>
#define TIMEOUT_SEC 120
struct timeval timeout;

int sock;
 
char **args;
char username[NAME_SIZE];
char opponentname[NAME_SIZE];
char opponentname2[NAME_SIZE];
int player_grid[GRID_SIZE][GRID_SIZE];
int opponent_grid[GRID_SIZE][GRID_SIZE];
int opponent_grid2[GRID_SIZE][GRID_SIZE];
// Inizializza le griglie a zero
void initialize_grids() {
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            player_grid[i][j] = 0;
            opponent_grid[i][j] = 0;
            opponent_grid2[i][j] = 0;
        }
    }
}

// Stampa la griglia di gioco
void print_grid(int grid[GRID_SIZE][GRID_SIZE], const char *title) {
    printf("\n%s\n", title);
    printf("  ");
    for (int i = 0; i < GRID_SIZE; i++)printf("%d ", i);
        printf("\n");
        for (int i = 0; i < GRID_SIZE; i++) {
           printf("%d ", i);
		   for (int j = 0; j < GRID_SIZE; j++) {
             if (grid[i][j] == 0){
                printf("~ ");
             }else if (grid[i][j] == 2){
                printf("M ");
             }else if (grid[i][j] == 3){
                printf("X ");
             }
           }
        
		printf("\n");
        }
	
}
// Aggiorna la griglia in base alla mossa effettuata e al risultato ricevuto dal server
void update_grid(int grid[GRID_SIZE][GRID_SIZE], int x, int y, int result) {
    if (result == 2){
        grid[x][y] = 2;
        printf("\n[GAME] Griglia viene aggiornata a 2\n\n"); // Mancato
    }
    else if (result == 3){
        grid[x][y] = 3;
        printf("\n[GAME] Griglia viene aggiornata a 3\n\n");// Colpito
    }
}
// funzione che permette al giocatore di riconnettersi al server
void play_again(){
	char again[2];
	printf("\n[GAME] La partita è terminata, vuoi giocare di nuovo? Digita: Y (YES) per continuare o qualunque altro tasto per chiudere \n\n");
         if (fgets(again, sizeof(again), stdin) != NULL) {
          // Verifica se la stringa termina con '\n'
            if (again[strlen(again) - 1] == '\n') {
              again[strlen(again)-1]='\0';
            }
         }

         if(strcmp(again, "Y") == 0 || strcmp(again, "y") == 0){
            //exec
           printf("\n[GAME] Cerco una nuova partita...\n\n");

        // Usa execv per rieseguire il programma
          close(sock);
          if (execv(args[0], args) == -1) {
            perror("Errore durante execv");
			exit(EXIT_FAILURE);
          }
         }
         else{
           printf("\n[GAME] Uscita dal gioco... \n\n");
           close(sock);
           exit(-1);
         }
}
//funzione che effettua la send di un messaggio di tipo char
bool send_data(int sock, const char *message) {
    ssize_t bytes_sent;

    // Invio del messaggio
    bytes_sent = send(sock, message, strlen(message), 0);
    if (bytes_sent == -1) {
        switch (errno) {
            case EPIPE:
                printf("[ERRORE] Socket chiuso dall'altra parte (EPIPE). Chiusura del client.\n");
                
                close(sock); // Chiude il socket perché non più utilizzabile
                return false;

            case ECONNRESET:
                printf("[ERRORE] Connessione interrotta bruscamente (ECONNRESET).\n");
                // Il client o server si è disconnesso in modo forzato
                close(sock);
                return false;

           // case EAGAIN:
            case EWOULDBLOCK:
                printf("[ERRORE] Socket non bloccante, invio non possibile ora (EAGAIN/EWOULDBLOCK).\n");
               
                return false;

            default:
                perror("[ERRORE] Errore durante l'invio dei dati");
                return false;
        }
    } else {
        // Invio riuscito
        printf("[SUCCESSO] Inviati %zd byte: %s\n", bytes_sent, message);
        return true;
    }
}
// funzione che effettua la reiceve con il timer e controlla che non ci siano errori
bool receive_data(int sock, char *msg, size_t msg_size) {
    fd_set read_fds;
    struct timeval timeout;

    // Imposta il timeout
    timeout.tv_sec = TIMEOUT_SEC;
    timeout.tv_usec = 0;

    // Inizializza il set di file descriptor
    FD_ZERO(&read_fds);
    FD_SET(sock, &read_fds);

    // Chiamata a select per attendere dati sul socket
    int retval = select(sock + 1, &read_fds, NULL, NULL, &timeout);

    if (retval == -1) {
        perror("[ERRORE] Errore in select");
        return false;
    } else if (retval == 0) {
        printf("[ERRORE] Tempo scaduto! Nessun dato ricevuto entro %d secondi.\n", TIMEOUT_SEC);
        return false;  // Timeout scaduto
    }

    // Se il socket è pronto, chiamiamo recv
    int bytes_received = recv(sock, msg, msg_size, 0);
    if (bytes_received == -1) {
        switch (errno) {
            case EPIPE:
                printf("[ERRORE] Socket chiuso dall'altra parte (EPIPE).\n");
                return false;

            case ECONNRESET:
                printf("[ERRORE] Connessione interrotta bruscamente (ECONNRESET).\n");
                return false;

            case EWOULDBLOCK:
                printf("[ERRORE] Nessun dato ricevuto al momento (EAGAIN/EWOULDBLOCK).\n");
                return false;

            default:
                perror("[ERRORE] Errore durante la ricezione dei dati");
                return false;
        }
    } else if (bytes_received == 0) {
        printf("[ERRORE] Connessione chiusa dall'altra parte.\n");
        return false;
    } else {
        msg[bytes_received] = '\0';  // Terminare la stringa ricevuta
        return true;
    }
}

//funzione che permette la chiusura del client e del socket associato al client
void client_handler(){
        close(sock);
        printf("\n\n[GAME] Il client ha abbandonato la partita\n\n");
        exit(-1);
}
// funzione che prende in input la porta in cui avviene la connessione e lo trasforma in un intero
int portvalidate(char *pstring){
    //funzione che mi assicura che siamo nel range corretto delle porte
    char *endpt;
    int port = strtol(pstring, &endpt, 10);
    //controlliamo che l'input era un numero valido
    if(*endpt != '\0'){
        return 1;
    } else if(port < 5001){
        return 2;
    } else if(port > 65535){
        return 3;
	}

    return 0;
}
//funzione che controlla la validità dell'indirizzo ip 
int ipcontrol(char *ip){
        //mediante la funzione inet_pton che converte ip in un valore binario
        struct sockaddr_in sa;
        if(inet_pton(AF_INET,ip,&(sa.sin_addr))!=1){
                return 1;
        }
        return 0;
}
// funzione che crea il socket del client con i parametri dati in input
int initSocket(char *port ,char *ipAddress){
        int ret;
        struct sockaddr_in serv_addr;
        if(ret=portvalidate(port)!=0){
                if(ret==1){
                        printf("non è valida la porta %s\n",port);
                        return 1;
                }
                if(ret==2){
                        printf("la porta %s ha valore minore di 5001\n",port);
                        return 1;
                }
                if(ret==3){
                        printf("la porta %s ha valore maggiore di 65535\n",port);
                        return 1;
                }
        }
        int porta=strtol(port,NULL,10);
        if(ret=ipcontrol(ipAddress)!=0){
                if(strcmp(ipAddress,"null") != 0){                                                                                                                            printf("l'indirizzo ip %s non e' corretto\n",ipAddress);
                  return 1;
                }
        }
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
           printf("\nErrore nella creazione della socket\n");
           return -1;
        }
		 memset(&serv_addr,0,sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(porta);
        if(ret!=0){
                serv_addr.sin_addr.s_addr=INADDR_ANY;
        }
        else{
             if (inet_pton(AF_INET,ipAddress, &serv_addr.sin_addr) <= 0) {
               printf("\nIndirizzo non valido\n");
               close(sock);
               return -1;
             }
        }
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                printf("\nConnessione fallita\n");
                close(sock);
                return -1;                                                                                
        }
        printf("\n\n[GAME] Connessione al server avvenuta correttamente, inizia la partita\n\n");
        return 0;
}
//funzione che gestisce il gioco a 2
void twoplayers(int opponentgrid[GRID_SIZE][GRID_SIZE],char opponentname[NAME_SIZE]){
        char msg[BUFFER_SIZE];
		int b,h;
       
	    
		
		printf("\n[GAME] Benvenuto nella versione per due giocatori\n\n");
        printf("\n[GAME] Siete rimasti solamente te ed il giocatore %s nella partita\n\n",opponentname);
        print_grid(opponentgrid, "\n[GAME] Griglia Avversario");

        while(1){
            char msgmossa[]="Tocca a te. Inserisci mossa (es. 4 5): ";

            // Riceve il messaggio
    
            if(!receive_data(sock,msg, sizeof(msg))){
			//twoplayers
			 
			 play_again();
			 
			 break;
		    }
                
	        printf("\n[GAME] La receive 2players è: %s \n\n",msg);
		
	      if(strstr(msg, "Attendi il tuo turno ")){
             printf("\n[GAME] Attendi il tuo turno\n\n");
				  
	         if(!receive_data(sock,msg, sizeof(msg))){
			//twoplayers
			   printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			   play_again();
			   
			   break;
		     }    
		  		  
		     if (strstr(msg, "Colpito") || strstr(msg, "Mancato.")){
			   printf("%s\n", msg);
		     }
          }

          if (strstr(msg, "Hai perso.") || strstr(msg, "Hai vinto!")) {
                  printf("\n[GAME] Fine partita \n\n");
                  printf("%s\n",msg);
				  // Visualizza risultato finale
                  play_again();
				  
				  break;
          }
				
          if(strstr(msg, msgmossa)){
					
				fd_set read_fds;
                riprova:
                // Inizializza il set di file descriptor e il timeout
                FD_ZERO(&read_fds);
                FD_SET(STDIN_FILENO, &read_fds);

                timeout.tv_sec = 30;  // Timeout 
                timeout.tv_usec = 0;

                printf("\n[GAME]Inserisci la tua mossa (hai 30 secondi):\n\n");

                // Uso select per monitorare lo stdin con un timeout
                int retval = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);

                if (retval == -1) {
                  perror("Errore in select");
                  break;  // Termina il programma in caso di errore
                } else if (retval == 0) {
                  printf("\n[GAME]Tempo scaduto! Il programma terminerà.\n");
                  break;  // Termina il programma in caso di timeout
                } else {
                // Input disponibile entro il timeout
                   if (fgets(msg, BUFFER_SIZE, stdin) != NULL) {
                 // Controlla se l'input è vuoto o mal formattato
                     if (msg[0] == '\n' || msg[0] == '\0' || sscanf(msg, "%d %d", &b, &h) != 2) {
                      printf("\n[GAME] Mossa vuota o mal formattata, riprova inserendo le coordinate in questo modo: x (spazio) y\n\n");
                      goto riprova;
                     }

            // Prepara il messaggio per l'invio al server
                     snprintf(msg, BUFFER_SIZE, "%d %d", b, h);

            // Controlla se la mossa è valida
                     if (b < 0 || b >= GRID_SIZE || h < 0 || h >= GRID_SIZE || opponentgrid[b][h] == 3 || opponentgrid[b][h] == 2) {
                       printf("\n[GAME] Mossa non valida, riprova: stai nei confini della mappa o scegli una casella non già scelta\n\n");
                       goto riprova;
                     }

            // Mossa valida: invia la mossa al server
                     printf("Mossa valida: %d %d\n", b, h);
                   } else {
                    printf("Errore nella lettura dell'input.\n");
                    
					break;  // Termina il programma in caso di errore
                   }
                }
	      
                if(send_data(sock, msg)==false){
				   play_again();
			       
				   break;
		        }

                if(!receive_data(sock,msg, sizeof(msg))){
			      printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			      play_again();
			      
				  break;
		        }  
			
            // Gestione dei messaggi ricevuti dal server
                if (strstr(msg, "Colpito!")) {
                   printf("Colpito!\n");
                   update_grid(opponentgrid, b, h, 3); // Marca la griglia avversaria con 'X'
                   print_grid(opponentgrid, "[GAME] Griglia Avversario");
                }else if (strstr(msg, "Mancato.")) {
                  printf("Mancato.\n");
                  update_grid(opponentgrid, b, h, 2); // Marca la griglia avversaria con 'M'
                  print_grid(opponentgrid, "[GAME] Griglia Avversario");
                }
          }
        }
           play_again();
		   pthread_exit(NULL);
           exit(-1);
}

//funzione che inizializza il gioco
void initgame(){
    int id;
    char ch;
    int bytes_received;
    
    bytes_received = recv(sock, &id, sizeof(id), 0);
    if(bytes_received<=0){
			//twoplayers
			play_again();
			exit(-1);
	}  

    printf("\n[GAME] Inserisci il tuo username di massimo 20 caratteri, quando anche il nemico ha fatto potrete giocare! \n\n ");
    fgets(username, NAME_SIZE, stdin);
    // condizione che elimina caratteri in eccesso per evitare che vadano a finire nella mossa successiva

    size_t len =strlen(username);
    if (len > 0 && username[len - 1] == '\n') {
       username[len - 1] = '\0';
    } else {
       while ((ch = getchar()) != '\n');
    }
 
	if(send_data(sock, username)==false){
			//twoplayers
			play_again();
			
			exit(-1);
	}
	
	
	if(!receive_data(sock, opponentname, NAME_SIZE)){
			//twoplayer
			 printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			 play_again();
			 
			 exit(-1);
    }   

    if(!receive_data(sock, opponentname2, NAME_SIZE)){
			//twoplayer
			 printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			 play_again();
			 
			 exit(-1);
    }

    printf("\n[GAME] Stai giocando la partita numero: %d contro %s id 1 e %s id 2 \n\n", id, opponentname, opponentname2);

    initialize_grids();
    print_grid(opponent_grid, "[GAME] Griglia Avversario");
    print_grid(opponent_grid2, "[GAME] Griglia Avversario 2");
}
// funzione che gestisce il gioco a 3
void *game_handler(){
    int n=9; // conta il numero di navi del giocatore corrente
    int n1=9;
    int n2=9;
    char buffer[BUFFER_SIZE];
	pthread_t thread_id;
    int x, y;
    int X, Y;
    int target;
    char current[2];
    char numclient[2];
    char coordinates[4];
//char again2[2];
    
    initgame();
    while (1){
       char msgmossa[]="Tocca a te. Inserisci mossa (es. 4 5): ";

       if(!receive_data(sock, numclient, sizeof(numclient))){
			//twoplayer
			 printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			 play_again();
			 break;
	   }  
	
       printf("\n[GAME] E' il turno del giocatore %s\n\n",numclient);
		
	   if(!receive_data(sock, buffer, sizeof(buffer))){
			//twoplayer
			 printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			 play_again();
			 break;
	   }

       //confronta stringa ricevuta ed attesa
       if(strstr(buffer, "Attendi il tuo turno")){

		printf("\n[GAME] Attendi il tuo turno\n\n");
        printf("\n[GAME] Fra poco riceverai le coordinate della mossa\n\n");

        if(!receive_data(sock, coordinates, sizeof(coordinates))){
			//twoplayer
			printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			play_again();
			break;
		} 
		
        sscanf(coordinates, "%d %d", &X, &Y);
        printf("\n[GAME] Le coordinate colpite sono %d e %d \n\n",X,Y);

        if(!receive_data(sock, buffer, sizeof(buffer))){
			//twoplayer
			 printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			 play_again();
			 break;
		}

        if(strstr(buffer, "L'avversario ti ha mancato.")){
           printf("%s\n", buffer);
        }

		if(strstr(buffer, "La tua nave è stata colpita!")){
                    printf("%s\n", buffer);
                    n--;
        }
				
		if(strstr(buffer, "L'avversario ha mancato.")){
                   printf("%s\n", buffer);

                   if(strcmp(numclient,"1")==0){
                            update_grid(opponent_grid2,X,Y,2);
                            print_grid(opponent_grid2, "[GAME] Griglia Avversario Colpito");
                   }

		           if(strcmp(numclient,"2")==0){
                         if(!receive_data(sock, current, sizeof(current))){
			             //twoplayer
			              printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			              play_again();
			              break;
		                 }                 
		
			             if (strcmp(current,"1")==0){
                                update_grid(opponent_grid,X,Y,2);
                                print_grid(opponent_grid, "Griglia Avversario Colpito");
                         }

			             else if(strcmp(current,"3")== 0){
                                     update_grid(opponent_grid2,X,Y,2);
                                     print_grid(opponent_grid2, "[GAME] Griglia Avversario Colpito");
			             }

		           }
                   if(strcmp(numclient,"3")==0){
                            update_grid(opponent_grid,X,Y,2);
                            print_grid(opponent_grid, "[GAME] Griglia Avversario Colpito");
                   }
       }

       if(strstr(buffer,"Una nave è stata colpita!")){
                    printf("%s\n",buffer);

                    if(strcmp(numclient,"1")==0){
                            n2--;
                            printf("navi rimanenti %d\n",n2);
                            update_grid(opponent_grid2,X,Y,3);
                            print_grid(opponent_grid2, "Griglia Avversario Colpito");
                    }
		    
		            if(strcmp(numclient,"2")==0){
                      if(!receive_data(sock, current, sizeof(current))){
			          //twoplayer
			           printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			           play_again();
			           break;
		              } 
                     
	                  if (strcmp(current,"1")==0){
                                n1--;
                                printf("navi rimanenti %d\n",n1);
                                update_grid(opponent_grid,X,Y,3);
                                print_grid(opponent_grid, "Griglia Avversario Colpito");
                      }
                            
			          else if(strcmp(current,"3")==0){
                                     n2--;
                                     printf("navi rimanenti %d\n",n2);
                                     update_grid(opponent_grid2,X,Y,3);
                                     print_grid(opponent_grid2, "Griglia Avversario Colpito");
                      }
                    }
                    
		            if(strcmp(numclient,"3")==0){
                            n1--;
                            printf("navi rimanenti %d\n",n1);
                            update_grid(opponent_grid,X,Y,3);
                            print_grid(opponent_grid, "Griglia Avversario Colpito");
                    }
                   
	   }
       } 
       if(strstr(buffer, msgmossa)){
        fd_set read_fds;
        
        riprova:
    // Inizializza il set di file descriptor e il timeout
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);

        timeout.tv_sec = 30;  // Timeout di 5 secondi
        timeout.tv_usec = 0;
		 
        printf("\n[GAME] Inserisci la tua mossa specificando dove e chi (esempio 0 0 1 ,se vuoi colpire posizione 0 0 giocatore 1 \n\n");
	    printf("\n[GAME] ATTENZIONE HAI SOLO 30 SECONDI DI TEMPO, SE SCADE SARAI ELIMINATO \n\n");
		 
	    int retval = select(STDIN_FILENO + 1, &read_fds, NULL, NULL, &timeout);

        if (retval == -1) {
          perror("Errore in select");
          break;  // Termina il programma in caso di errore
        }  
	    else if (retval == 0) {
          printf("\n[GAME] Tempo scaduto! Il programma terminerà.\n\n");
          break;  // Termina il programma in caso di timeout
        } 
	    else {
        // Input disponibile entro il timeout
           if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
            // Controlla se l'input è vuoto o mal formattato
             if (buffer[0] == '\n' || buffer[0] == '\0' || sscanf(buffer, "%d %d %d", &x, &y, &target) != 3) {
                printf("\n[GAME] Mossa vuota o mal formattata, riprova inserendo le coordinate in questo modo: x (spazio) y target\n\n");
                goto riprova;
             }
			 snprintf(buffer, BUFFER_SIZE, "%d %d %d", x, y, target);

            // Controlla se il target è valido (1 o 2)
             if (target < 1 || target > 2) {
                printf("\n[GAME] Hai solo due giocatori possibili, inserire o 1 o 2\n\n");
                goto riprova;
             }

            // Controlla le coordinate per il giocatore 1
             if (target == 1) {
                if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE || opponent_grid[x][y] == 3 || opponent_grid[x][y] == 2) {
                    printf("\n[GAME] Mossa non valida, riprova: stai nei confini della mappa o scegli una casella non già scelta\n\n");
                    goto riprova;
                }
             }
             // Controlla le coordinate per il giocatore 2
             else if (target == 2) {
                if (x < 0 || x >= GRID_SIZE || y < 0 || y >= GRID_SIZE || opponent_grid2[x][y] == 3 || opponent_grid2[x][y] == 2) {
                    printf("\n[GAME] Mossa non valida, riprova: stai nei confini della mappa o scegli una casella non già scelta\n\n");
                    goto riprova;
                }
             }
           }
		   else {
             printf("Errore nella lettura dell'input.\n");
             break;  // Termina il programma in caso di errore
           }
	    }

		 printf("\n[GAME] Mossa valida, hai inserito: x = %d, y = %d, target = %d, ora send \n\n", x, y, target);
         if(send_data(sock, buffer)==false){
			//twoplayers
			break;
		 }


         if(!receive_data(sock, buffer, sizeof(buffer))){
			//twoplayer
			 printf("[DEBUG] Nessun dato ricevuto, chiusura della comunicazione.\n");
			 play_again();
			 break;
		 } 
	    
	 // Gestione dei messaggi ricevuti dal server
         if (strstr(buffer, "Colpito!")) {
              printf("\n[GAME] Colpito!\n\n");
              if(target==1){
                n1--;
                printf("\n[GAME] Navi rimanenti %d\n\n",n1);
                update_grid(opponent_grid, x, y, 3); // Marca la griglia avversaria con 'X'
                print_grid(opponent_grid, "[GAME]Griglia Avversario 1");
              }
              else if(target==2){
                n2--;
                printf("\n[GAME] Navi rimanenti %d\n\n",n2);
                update_grid(opponent_grid2, x, y, 3); // Marca la griglia avversaria con 'X'
                print_grid(opponent_grid2, "[GAME]Griglia Avversario 2");
              }
         }
         else if (strstr(buffer, "Mancato.")) {
                printf("\n[GAME] Mancato.\n\n");
              
		        if(target==1){
                  update_grid(opponent_grid, x, y, 2); // Marca la griglia avversaria con 'M'
                  print_grid(opponent_grid, "[GAME]Griglia Avversario 1");
                }
                else if(target==2){
                  update_grid(opponent_grid2, x, y, 2); // Marca la griglia avversaria con 'X'
                  print_grid(opponent_grid2, "[GAME]Griglia Avversario 2");
                }
         }
      
       }

       printf("\n[GAME] Resoconto navi e' il seguente il giocatore corrente ha %d navi l'opponent 1 ha %d navi e l'opponent 2 ha %d navi\n\n",n,n1,n2);

       if(n==0){
	     printf("\n[GAME] Tutte le tue navi sono state abbattute\n\n");
	     printf("\n[GAME] Hai perso la partita,sei stato eliminato\n\n");
	     play_again();
         printf("\n[GAME] Uscita dal gioco... \n\n");
	     
		 break;
		 
       }
	   else if(n1==0){
         printf("\n[GAME] Il giocatore avversario %s ha abbandonato la partita, perchè sono state eliminate le sue navi\n\n", opponentname);
         twoplayers(opponent_grid2,opponentname2);
       }
	   else if(n2==0){
         printf("\n[GAME] Il giocatore avversario %s ha abbandonato la partita, perchè sono state eliminate tutte le sue navi\n\n",opponentname2);
         twoplayers(opponent_grid,opponentname);
       }
    }
    
	pthread_exit(NULL);
}

int main(int argc,char **argv) {
    pthread_t thread_id;
    int ret;

    args=argv;

    if(argc!=3){
        printf("non hai inserito il numero corretto di parametri\n");
        exit(-1);
    }
    if(initSocket(argv[1],argv[2]) != 0){
        printf("Errore: non è possibile connettersi al server\n");
        exit(-1);
    }
    signal(SIGINT,client_handler);
	signal(SIGTERM,client_handler);
    signal(SIGPIPE, SIG_IGN);
    

    ret=pthread_create(&thread_id, NULL, game_handler,NULL);
    if(ret!=0){
         printf("errore nella creazione del thread\n");
         exit(-1);
    }
    pthread_join(thread_id,NULL);
    return 0;
}

