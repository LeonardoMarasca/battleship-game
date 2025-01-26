#include<stdio.h>
#include<stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <signal.h>
#define PORT 8080
#define BUFFER_SIZE 1024
#define GRID_SIZE 9
#define MAX_SHIPS 9
#define NAME_SIZE 20
#include <errno.h>
#include <semaphore.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <sys/socket.h>

int server_socket;
int numclient=0;

struct sembuf oper;

typedef struct {
    int player1_socket;
    int player2_socket;
    int player3_socket;
    int game_id;
    int grid1[GRID_SIZE][GRID_SIZE];
    int grid2[GRID_SIZE][GRID_SIZE];
    int grid3[GRID_SIZE][GRID_SIZE];
    int ships1, ships2, ships3; // Contano le navi rimanenti per ogni giocatore
} Game;

typedef struct {
        int current_player;
        int other_player;
        int other_player2;
        int x,y;
        char coordinates[4];
        int (*opponent_grid)[GRID_SIZE][GRID_SIZE];
        int target_fd;
		int terminate_flag;
        int sd;
        int ret;
        int t;
} Data;

typedef struct {
  int current_player;
  int target;
  int (*opponent_grid)[GRID_SIZE][GRID_SIZE];
  int x, y ;
  int terminate_flag2;
  int sd;        
  int ret;
} Data2;

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
        return true;
    }
}

bool send_int(int sock, int id) {
    ssize_t bytes_sent;

    // Invio del messaggio contenente un intero
    bytes_sent = send(sock, &id, sizeof(id), 0);
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
        return true;
    }
}

bool receive_data(int bytes_received) {

    // Riceve i dati nel buffer
    if (bytes_received == -1) {
        switch (errno) {
            case EPIPE:
                printf("[ERRORE] Socket chiuso dall'altra parte (EPIPE). Chiusura del client.\n");
            //    close(sock); // Chiude il socket perché non più utilizzabile
                return false;

            case ECONNRESET:
                printf("[ERRORE] Connessione interrotta bruscamente (ECONNRESET).\n");
                // Il client o server si è disconnesso in modo forzato
          //      close(sock);
                return false;

            case EWOULDBLOCK:
                printf("[ERRORE] Socket non bloccante, ricezione non possibile ora (EAGAIN/EWOULDBLOCK).\n");
                return false;

            default:
                perror("[ERRORE] Errore durante la ricezione dei dati");
                return false;
        }
    } else if (bytes_received == 0) {
        // Se bytes_received == 0, significa che la connessione è stata chiusa
        printf("[ERRORE] Connessione chiusa dall'altra parte.\n");
        //close(sock);
        return false;
    } else {
        return true;
    }
}

void initialize_grid(int grid[GRID_SIZE][GRID_SIZE]) {
	//inizializza le griglie impostandole a 0
    for (int i = 0; i < GRID_SIZE; i++){
        for (int j = 0; j < GRID_SIZE; j++){
            grid[i][j] = 0;
		 }
    }
}

void place_ships(int grid[GRID_SIZE][GRID_SIZE], int *ships) {
    *ships = MAX_SHIPS;
    // Posiziona navi casualmente
    for (int i = 0; i < MAX_SHIPS; i++) {
        int x = rand() % GRID_SIZE;
        int y = rand() % GRID_SIZE;
                if (grid[x][y] == 0) {
                  grid[x][y] = 1; // Posiziona una nave
                } else {
                i--; // Ripeti se la cella è già occupata
                }
        }
}

bool check_elimination(int ships){
	//controlla il numero di navi rimanenti
    return ships <= 0;
}

void print_grid(int grid[GRID_SIZE][GRID_SIZE], const char *title) {
	//stampa la griglia
    printf("\n%s\n", title);
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
                printf("%d ", grid[i][j]);
        }
        printf("\n");
  }
 }


void *currentHandler(void *new_data){

  Data *data = (Data *)new_data;
  
  while(!data->terminate_flag){
    wait1:
    oper.sem_num = 1;  // Primo semaforo
    oper.sem_op = -1;  // Decrementa di 1
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto wait1;
    }
         
    if(data->terminate_flag){
    	break;
	}
	
	if(send_data(data->current_player, "Tocca a te. Inserisci mossa (es. 4 5): ")==false){
		break;
	}
	 
	//passa a opponent con una signal
    signal1:
    oper.sem_num = 2;
    oper.sem_op = 1;
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto signal1;
    }

    wait2:  //wait gettone
    oper.sem_num = 1;
    oper.sem_op = -1;
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto wait2;
    }

    if ((*data->opponent_grid)[data->x][data->y] == 3) {
      if(data->terminate_flag){
		 break;
	  }
	  if(send_data(data->current_player, "Colpito!")==false){
			free(data);
			break;
		}
    }
	  
	if((*data->opponent_grid)[data->x][data->y]==2) {
      if(data->terminate_flag){
		 break;
	  }
      if(send_data(data->current_player, "Mancato")==false){
			free(data);
			break;
		}
	}
	 
	signal2:
    oper.sem_num = 2; 
    oper.sem_op = 1;  
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto signal2;
    }
  }
 pthread_exit(NULL);
}

void *OppHandler3(void *new_data){
   Data *data = (Data *)new_data;
  
   while(!data->terminate_flag){
    wait3: //wait gettone
    oper.sem_num = 2;  
    oper.sem_op = -1;  // Decrementa di 1
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto wait3;
    }
    
	//esce dal thread se il gioco è terminato prima di fare la send
	if(data->terminate_flag){
		 break;
	  }
    if(send_data(data->other_player, "Attendi il tuo turno")==false){
			break;
	}
	
    signal3: //signal a opponent2
    oper.sem_num = 3;  
    oper.sem_op = 1;  
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto signal3;
    }
      
    wait4:  //wait
    oper.sem_num = 2; 
	oper.sem_op = -1;  
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto wait4;
    }
   
    if(send_data(data->other_player, data->coordinates)==false){	
			break;
	}
		
    signal4:  //signal a opponent2
    oper.sem_num = 3; 
    oper.sem_op = 1;  
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto signal4;
    }
        
    wait5: //wait
    oper.sem_num = 2;  
    oper.sem_op = -1;  
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto wait5;
    }
      
    if(data->target_fd == data->other_player){
     if ((*data->opponent_grid)[data->x][data->y] == 3) {
 
	  if(data->terminate_flag){
		 break;
	  }
	  if(send_data(data->target_fd, "La tua nave è stata colpita!")==false){		
			break;
	  }
     }
     
	 if((*data->opponent_grid)[data->x][data->y] == 2) {
 
      if(data->terminate_flag){
		 break;
	  }
      if(send_data(data->target_fd, "L'avversario ti ha mancato.")==false){	
			break;
	  }
	 }
 
     signal5:
     oper.sem_num = 3;  
     oper.sem_op = 1;  
     oper.sem_flg = 0;
	 data->ret = semop(data->sd,&oper,1);
     if (data->ret == -1 && errno == EINTR){
        goto signal5;
     }
    }

    else{
     if ((*data->opponent_grid)[data->x][data->y] == 3) {
      if(data->terminate_flag){
		 break;
	  }
	  if(send_data(data->other_player, "Una nave è stata colpita!")==false){		
			break;
	  }
     }
     if((*data->opponent_grid)[data->x][data->y]==2) {
      if(data->terminate_flag){
		 break;
	  }
      if(send_data(data->other_player, "L'avversario ha mancato.")==false){		
			break;
	  }
     }

     signal6:
     oper.sem_num = 3;  // Primo semaforo
     oper.sem_op = 1;  // Decrementa di 1
     oper.sem_flg = 0;
     data->ret = semop(data->sd,&oper,1);
     if (data->ret == -1 && errno == EINTR){
        goto signal6;
     }
    }
  }
  pthread_exit(NULL);
}

void *Opp2Handler3(void *new_data){
  Data *data = (Data *)new_data;

  while(!data->terminate_flag){
    wait6:
    oper.sem_num = 3;
    oper.sem_op = -1;  
	oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto wait6;
    }
         
    if(data->terminate_flag){
		 break;
	}
    if(send_data(data->other_player2, "Attendi il tuo turno")==false){
		break;
	}
	
    signal7:  //signal a gameHandler
    oper.sem_num = 0;
    oper.sem_op = 1;
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto signal7;
    }

    wait7:   //wait
    oper.sem_num = 3;
    oper.sem_op = -1;
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto wait7;;
    }
   
    if(data->terminate_flag){
		 break;
	}
    if(send_data(data->other_player2, data->coordinates)==false){
		break;
	}
	
    signal8:  //signal a game handler
    oper.sem_num = 0;
    oper.sem_op = 1;
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto signal8;
    }
	 
	wait8:
    oper.sem_num = 3;
    oper.sem_op = -1;
    oper.sem_flg = 0;
    data->ret = semop(data->sd,&oper,1);
    if (data->ret == -1 && errno == EINTR){
        goto wait8;
    }

    if(data->target_fd == data->other_player2){
     if ((*data->opponent_grid)[data->x][data->y] == 3) {
      if(data->terminate_flag){
		 break;
	  }
      if(send_data(data->target_fd, "La tua nave è stata colpita!")==false){
		break;
	  }
	 }
     
	 if((*data->opponent_grid)[data->x][data->y]==2) {
      if(data->terminate_flag){
		 break;
	   }
      if(send_data(data->target_fd, "L'avversario ti ha mancato.")==false){
		break;
	  }
	 }
   
     signal9:
     oper.sem_num = 0;
     oper.sem_op = 1;
     oper.sem_flg = 0;
     data->ret = semop(data->sd,&oper,1);
     if (data->ret == -1 && errno == EINTR){
        goto signal9;
     }
    }

    else{
     if ((*data->opponent_grid)[data->x][data->y] == 3) {
       if(data->terminate_flag){
		 break;
	   }
       if(send_data(data->other_player2, "Una nave è stata colpita!")==false){		
			break;
	   }
	 }
     if((*data->opponent_grid)[data->x][data->y]==2) {
      if(data->terminate_flag){
		 break;
	  }
      if(send_data(data->other_player2, "L'avversario ha mancato.")==false){
		break;
	  }    
     }

     signal10:
     oper.sem_num = 0;
	 oper.sem_op = 1;
     oper.sem_flg = 0;
     data->ret = semop(data->sd,&oper,1);
     if (data->ret == -1 && errno == EINTR){
        goto signal10;
     }
    }
   }
 pthread_exit(NULL);
}

void *current2handler(void *new_data2){
    Data2 *data2 = (Data2 *)new_data2;

    while(!data2->terminate_flag2){

     wait9:
     oper.sem_num = 1;
     oper.sem_op = -1;
     oper.sem_flg = 0;
     data2->ret = semop(data2->sd,&oper,1);
     if (data2->ret == -1 && errno == EINTR){
        goto wait9;
     }

     if(data2->terminate_flag2){
		 break;
	  }
     if(send_data(data2->current_player, "Tocca a te. Inserisci mossa (es. 4 5): ")==false){
		break;
	 } 
     
     signal11: //signal a opponent v2
     oper.sem_num = 2;
     oper.sem_op = 1;
     oper.sem_flg = 0;
	 data2->ret = semop(data2->sd,&oper,1);
     if (data2->ret == -1 && errno == EINTR){
        goto signal11;
     }
        
     wait10:
     oper.sem_num = 1;
     oper.sem_op = -1; 
     oper.sem_flg = 0;
     data2->ret = semop(data2->sd,&oper,1);
     if (data2->ret == -1 && errno == EINTR){
        goto wait10;
     }

     if ((*data2->opponent_grid)[data2->x][data2->y] == 3) {
         
       if(data2->terminate_flag2){
		 break;
	   }
       if(send_data(data2->current_player, "Colpito!")==false){
		break;
	   }         
	 }
     
	 if((*data2->opponent_grid)[data2->x][data2->y]==2) {            
       if(data2->terminate_flag2){
		 break;
	   }
       if(send_data(data2->current_player, "Mancato.")==false){
	    	break;
	   }  
     }
        
     signal12:
     oper.sem_num = 2;
     oper.sem_op = 1;
     oper.sem_flg = 0;
     data2->ret = semop(data2->sd,&oper,1);
     if (data2->ret == -1 && errno == EINTR){
        goto signal12;
     }
    }
    pthread_exit(NULL);
}

void *opp2handler(void *new_data2){
        Data2 *data2 = (Data2 *)new_data2;
	
    while(!data2->terminate_flag2){
     wait11:
     oper.sem_num = 2;
     oper.sem_op = -1;
     oper.sem_flg = 0;
     data2->ret = semop(data2->sd,&oper,1);
     if (data2->ret == -1 && errno == EINTR){
        goto wait11;
     }

     if(data2->terminate_flag2){
		 break;
	 }
     if(send_data(data2->target, "Attendi il tuo turno ")==false){
			break;
	 }  
				  
     signal13:  //signal a twoplayers
     oper.sem_num = 0;
     oper.sem_op = 1;
     oper.sem_flg = 0;
     data2->ret = semop(data2->sd,&oper,1);
     if (data2->ret == -1 && errno == EINTR){
        goto signal13;
     }
    
	 wait12:
     oper.sem_num = 2;
     oper.sem_op = -1;
     oper.sem_flg = 0;
     data2->ret = semop(data2->sd,&oper,1);
     if (data2->ret == -1 && errno == EINTR){
        goto wait12;
     }
     
	 if ((*data2->opponent_grid)[data2->x][data2->y] == 3) {
	   if(data2->terminate_flag2){
		 break;
	   }
       if(send_data(data2->target, "Colpito")==false){
	    	break;
	   }  
	 }
	 
     if ((*data2->opponent_grid)[data2->x][data2->y]==2) {
	   if(data2->terminate_flag2){
		 break;
	   }
       if(send_data(data2->target, "Mancato.")==false){
			break;
       }  
	 }
     
	 signal14:
     oper.sem_num = 0;
     oper.sem_op = 1;
     oper.sem_flg = 0;
     data2->ret = semop(data2->sd,&oper,1);
     if (data2->ret == -1 && errno == EINTR){
        goto signal14;
     }
    }
    pthread_exit(NULL);
}

void twoplayers(int target,int current,int (*opponent_grid)[GRID_SIZE][GRID_SIZE],int *opponent_ships,int (*current_grid)[GRID_SIZE][GRID_SIZE],int *current_ships){

  Data2 *new_data2 = malloc(sizeof(Data2));
       if (!new_data2) {
            perror("Malloc failed");
            exit(-1);
       }
  char buff[BUFFER_SIZE];
  int b, h, i;
  new_data2->terminate_flag2=0;

  printf("\nBenvenuti nella versione per due giocatori\n");
  print_grid(*current_grid, "Griglia giocatore 1");
  print_grid(*opponent_grid, "Griglia giocatore 2");

  new_data2->target = target;
  new_data2->current_player = current;
  new_data2->opponent_grid = opponent_grid;

  pthread_t threads2[2];

  //creo semafori
  new_data2->sd = semget(IPC_PRIVATE, 3,IPC_CREAT|0666);
  if (new_data2->sd== -1){
	printf("semget error (2)\n");
	free(new_data2);
    printf("Effettuata free di new_data2 in twoplayers\n");
    exit(-1);
  }

  new_data2->ret = semctl(new_data2->sd,0,SETVAL,1); //imposto il semaforo iniziale a 1  (;extra)
  if(new_data2->ret== -1){
   printf("semctl error (2)\n");
   free(new_data2);
   printf("Effettuata free di new_data2 in twoplayers\n");
   exit(-1);
  }

  for ( i = 1; i < 3; i++) { //inizializzo i valori dei semafori a 0
   new_data2->ret = semctl(new_data2->sd,i,SETVAL,0); ; //;extra
   if(new_data2->ret == -1){
   printf("semctl error (1)\n");
   free(new_data2);
   printf("Effettuata free new_data2 twoplayers\n");
   exit(-1);
   }
  }
  new_data2->ret= pthread_create(&threads2[0], NULL, current2handler, (void*)new_data2);
  if(new_data2->ret!=0){
    printf("Errore nella creazione del thread2\n");
    free(new_data2);
    printf("Effettuata la free di new_data2 in twoplayers\n");
    exit(-1);
  }

  new_data2->ret = pthread_create(&threads2[1], NULL, opp2handler, (void*)new_data2);
  if(new_data2->ret!=0){
      printf("Errore nella creazione del thread2\n");
      free(new_data2);
      printf("Effettuata la free di new_data2 in twoplayers\n"); 
      exit(-1);
    }
  
  waitP:
  oper.sem_num = 0;
  oper.sem_op = -1;
  oper.sem_flg = 0;
  new_data2->ret = semop(new_data2->sd,&oper,1);
  if ( new_data2->ret== -1 && errno == EINTR){
        goto waitP;
  }

  while(1){
  
  signal15:  //signal a current
  oper.sem_num = 1;
  oper.sem_op = 1;
  oper.sem_flg = 0;
  new_data2->ret = semop(new_data2->sd,&oper,1);
  if ( new_data2->ret== -1 && errno == EINTR){
        goto signal15;
  }
  
  wait13:
  oper.sem_num = 0;  
  oper.sem_op = -1;  
  oper.sem_flg = 0;
  new_data2->ret = semop(new_data2->sd,&oper,1);
  if (new_data2->ret == -1 && errno == EINTR){
        goto wait13;
  }

  int bytes_received = recv(new_data2->current_player, buff, sizeof(buff), 0);  // Riceve mossa
  if(receive_data(bytes_received)==false){ //controlla l'esito della receive
			close(new_data2->current_player);
			break;
  }
  
  buff[bytes_received] = '\0';
  sscanf(buff, "%d %d", &b, &h); // Converte input in coordinate
  printf("la mossa ricevuta è %s \n", buff);

  new_data2->x=b;
  new_data2->y=h;
  // calcola l'esito della mossa
  if ((*new_data2->opponent_grid)[b][h] == 1) {
            (*new_data2->opponent_grid)[b][h] = 3; // Colpito
            (*opponent_ships)--;
  }
  else if((*new_data2->opponent_grid)[b][h]==0) {
            (*new_data2->opponent_grid)[b][h] = 2; // Mancato
  }
  new_data2->opponent_grid = opponent_grid;

  signal16:
  oper.sem_num = 1;
  oper.sem_op = 1;
  oper.sem_flg = 0;
  new_data2->ret= semop(new_data2->sd,&oper,1);
  if (new_data2->ret == -1 && errno == EINTR){
        goto signal16;
  }
 
  wait14:
  oper.sem_num = 0;
  oper.sem_op = -1;
  oper.sem_flg = 0;
  new_data2->ret = semop(new_data2->sd,&oper,1);
     if ( new_data2->ret== -1 && errno == EINTR){
        goto wait14;
  }

  if (check_elimination(*opponent_ships)) { //dopo l'esito mossa e aggiornamento navi controlla se la partita termina
    new_data2->terminate_flag2= 1;
	if(send_data(new_data2->current_player, "Hai vinto!")==false){
	    break;
	} 
    if(send_data(new_data2->target, "Hai perso.")==false){
			break;
	} 
			
	if (semctl(new_data2->sd, 0, IPC_RMID) == -1) {
               perror("Errore nella rimozione del semaforo");
			   exit(-1);
    }

    printf("Set di semafori rimosso correttamente.\n");
        break;
    }
   // Scambio dei ruoli tra giocatori a fine turno
    int temp = new_data2->current_player;
    new_data2->current_player = new_data2->target;
    new_data2->target = temp;

    // scambio di navi
    int temp1=*current_ships;
    *current_ships = *opponent_ships;
    *opponent_ships = temp1;
		
    for (int i = 0; i < GRID_SIZE; i++) {
      for (int j = 0; j < GRID_SIZE; j++) {
          int temp_value = (*current_grid)[i][j];
          (*current_grid)[i][j] = (*opponent_grid)[i][j];
          (*opponent_grid)[i][j] = temp_value;
       }
    }
    new_data2->opponent_grid = opponent_grid;
  }
 
  close(current);
  close(target);
  free(new_data2);
  printf("Fatta la free di new_data2 in twoplayers\n");
  pthread_exit(NULL);
  exit(-1);
}

bool creaThreads(Data *new_data){
  int ret;
  pthread_t threads[3];

  ret=pthread_create(&threads[0], NULL, currentHandler, (void*)new_data);
        if(ret!=0){
                printf("Errore nella creazione del thread\n");
	        	free(new_data);
                printf("Fatta free new_data crea_threads\n");
                return false;
        }

  ret=pthread_create(&threads[1], NULL, OppHandler3, (void*)new_data);
        if(ret!=0){
                printf("Errore nella creazione del thread\n");
		        free(new_data);
                printf("Fatta free new_data crea_threads\n");
                return false;
        }

  ret=pthread_create(&threads[2], NULL, Opp2Handler3, (void*)new_data);
        if(ret!=0){
                printf("Errore nella creazione del thread\n");
		        free(new_data);
                printf("Fatta free new_data crea_threads\n");
                return false;
        }
   return true;
}

void *game_handler(void *game_data){
  Game *game = (Game *)game_data;
  Data *new_data = malloc(sizeof(Data));
        if (!new_data) {
            perror("Malloc failed");
            exit(-1);
        }
		
  new_data->terminate_flag=0;
	
  int player1 = game->player1_socket;
  int player2 = game->player2_socket;
  int player3 = game->player3_socket;
  char buffer[BUFFER_SIZE];
  char nome1[NAME_SIZE], nome2[NAME_SIZE], nome3[NAME_SIZE];
  
  new_data->current_player = player1;
  new_data->other_player = player2;
  new_data->other_player2 = player3;
  
  int id, i;
  int target;
  int not_target;
  int (*current_grid)[GRID_SIZE][GRID_SIZE] ;
  int *current_ships;
  int *opponent_ships;
  int (*notopponent_grid)[GRID_SIZE][GRID_SIZE] ;
  int *notopponent_ships;

  //crea semafori
  new_data->sd = semget(IPC_PRIVATE, 4,IPC_CREAT|0666);
  if (new_data->sd == -1){
   printf("semget error (1)\n");
   free(new_data);
   printf("Effettuata la free di new_data in game_handler\n");
   exit(-1);
  }

  new_data->ret= semctl(new_data->sd,0,SETVAL,1); ;
  if( new_data->ret== -1){
   printf("semctl error (1)\n");
   free(new_data);
   printf("Effettuata la free di new_data in game_handler\n");
   exit(-1);
  }

  for ( i = 1; i < 4; i++) {
   new_data->ret = semctl(new_data->sd,i,SETVAL,0); ;
   if( new_data->ret== -1){
   printf("semctl error (1)\n");
   free(new_data);
   printf("Effettuata la free di new_data in game_handler\n");
   exit(-1);
   }
  }

   wait15:
   oper.sem_num = 0;
   oper.sem_op = -1;
   oper.sem_flg = 0;
   new_data->ret = semop(new_data->sd,&oper,1);
   if ( new_data->ret== -1 && errno == EINTR){
        goto wait15;
   }

   initialize_grid(game->grid1);
   initialize_grid(game->grid2);
   initialize_grid(game->grid3);
   place_ships(game->grid1, &game->ships1);
   place_ships(game->grid2, &game->ships2);
   place_ships(game->grid3, &game->ships3);

   id = game->game_id;

   if(send_int(player1, id)==false){
	free(new_data);
     printf("Effettuata la free di new_data in game_handler\n");
			exit(-1);
   }  

   if(send_int(player2, id)==false){
	  free(new_data);
      printf("Effettuata la free di new_data in game_handler\n");
	  exit(-1);
   } 
	
   if(send_int(player3, id)==false){
	 free(new_data);
     printf("Effettuata la free di new_data in game_handler\n");
	 exit(-1);
   } 
	
   int bytes_received = recv(new_data->current_player, nome1, NAME_SIZE, 0);
   if(receive_data(bytes_received)==false){
			close(new_data->current_player);
			free(new_data);
   printf("Effettuata la free di new_data in game_handler\n");
			exit(-1);
   }   

   if(send_data(player2, nome1)==false){
		free(new_data);
        printf("Effettuata la free di new_data in game_handler\n");
		exit(-1);
   } 
        
   if(send_data(player3, nome1)==false){
		free(new_data);
        printf("Effettuata la free di new_data in game_handler\n");
		exit(-1);
		} 
		
	bytes_received = recv(new_data->other_player, nome2, NAME_SIZE, 0);
    if(receive_data(bytes_received)==false){
				close(new_data->other_player);
		        free(new_data);
                printf("Effettuata la free di new_data in game_handler\n");
			    exit(-1);
	}

    if(send_data(player1, nome2)==false){
      free(new_data);
      printf("Effettuata la free di new_data in game_handler\n");
	  exit(-1);
	} 
		
	if(send_data(player3,nome2)==false){
		free(new_data);
        printf("Effettuata la free di new_data in game_handler\n");
		exit(-1);
	} 

    bytes_received = recv(new_data->other_player2, nome3, NAME_SIZE, 0);
    if(receive_data(bytes_received)==false){
			close(new_data->other_player2);
			free(new_data);
            printf("Effettuata la free di new_data in game_handler\n");
			exit(-1);
	}

    if(send_data(player1, nome3)==false){
		free(new_data);
        printf("Effettuata la free di new_data in game_handler\n");
		exit(-1);
		}
		
    if(send_data(player2, nome3)==false){
		free(new_data);
        printf("Effettuata la free di new_data in game_handler\n");
		exit(-1);
		} 

    int len = strlen(nome1);
    if (nome1[len - 1] == '\n') {
        nome1[len - 1] = '\0';
        len--;
    }

    len = strlen(nome2);
    if (nome2[len - 1] == '\n') {
        nome2[len - 1] = '\0';
    }
    len = strlen(nome3);
    if (nome3[len - 1]== '\n'){
        nome3[len - 1] = '\0';
    }
    
	printf("Nella partita numero %d giocano %s (giocatore 1) contro %s (giocatore2) contro %s (giocatore3)\n",id, nome1, nome2,nome3);

    if(creaThreads(new_data)==false){
		printf("Libero new data causa crethreads fail\n");
		free(new_data);
		exit(-1);
	}

    pthread_t thread_id = pthread_self();
    printf("In esecuzione sul thread: %lu\n", (unsigned long)thread_id);
	printf("Mappe della partita numero: %d\n", id);
	print_grid(game->grid1, "Griglia giocatore 1");
    print_grid(game->grid2, "Griglia giocatore 2");
    print_grid(game->grid3, "Griglia giocatore 3");

    while(1){
        //definisco qui current player e other player
        if(new_data->current_player == player1){
		 if(send_data(new_data->current_player,"1")==false){
			break;
		 } 
		 if(send_data(new_data->other_player, "1")==false){
	     	break;
		 } 
		 if(send_data(new_data->other_player2, "1")==false){
			break;
		 } 
        }
       
	   if(new_data->current_player == player2){
        if(send_data(new_data->current_player,"2")==false){
			break;
		} 
		if(send_data(new_data->other_player, "2")==false){
			break;
		} 
		if(send_data(new_data->other_player2, "2")==false){
			break;
		} 
	  }
        
	  if(new_data->current_player == player3){
        if(send_data(new_data->current_player,"3")==false){
				break;
		} 
		if(send_data(new_data->other_player, "3")==false){
			break;
		} 
		if(send_data(new_data->other_player2, "3")==false){
			break;
		} 
	   }

     //signal a current
     signal17:
     oper.sem_num = 1;
     oper.sem_op = 1;
     oper.sem_flg = 0;
     new_data->ret = semop(new_data->sd,&oper,1);
     if (new_data->ret == -1 && errno == EINTR){
        goto signal17;
     }
   
     wait16:
     oper.sem_num = 0;
     oper.sem_op = -1;
     oper.sem_flg = 0;
     new_data->ret = semop(new_data->sd,&oper,1);
	 if ( new_data->ret== -1 && errno == EINTR){
        goto wait16;
     }

     int bytes_received = recv(new_data->current_player, buffer, sizeof(buffer), 0); //riceve mossa
     if(receive_data(bytes_received)==false){
    		close(new_data->current_player);
			break;
		}   
		
     buffer[bytes_received] = '\0';// Converte input in coordinate che sono o shared o global. serve che i figli le possano inviare
     sscanf(buffer, "%d %d %d", &new_data->x, &new_data->y, &target); 

     if(new_data->current_player==player1){
                if(target==1){
                         current_grid=&game->grid1;
                         current_ships=&game->ships1;
                         new_data->opponent_grid=&game->grid2;
                         opponent_ships=&game->ships2;
                         notopponent_grid=&game->grid3;
                         notopponent_ships=&game->ships3;
                         new_data->target_fd=player2;
                         not_target=player3;
                }
                else if(target==2){
                         current_grid=&game->grid1;
                         current_ships=&game->ships1;
                         new_data->opponent_grid=&game->grid3;
                         opponent_ships=&game->ships3;
                         notopponent_grid=&game->grid2;
                         notopponent_ships=&game->ships2;
                         new_data->target_fd=player3;
                         not_target=player2;

                }
        }
     
	 if(new_data->current_player==player2){
                if(target==1){
                         current_grid=&game->grid2;
                         current_ships=&game->ships2;
                         new_data->opponent_grid=&game->grid1;
                         opponent_ships=&game->ships1;
                         notopponent_grid=&game->grid3;
						 notopponent_ships=&game->ships3;
                         new_data->target_fd=player1;
                         not_target=player3;
                         new_data->t=1;
                }
                else if(target==2){
                         current_grid=&game->grid2;
                         current_ships=&game->ships2;
                         new_data->opponent_grid=&game->grid3;
                         opponent_ships=&game->ships3;
                         notopponent_grid=&game->grid1;
                         notopponent_ships=&game->ships1;
                         new_data->target_fd=player3;
                         not_target=player1;
                         new_data->t=3;
                }
        }
     
	 if(new_data->current_player==player3){
                if(target==1){
                         current_grid=&game->grid3;
                         current_ships=&game->ships3;
                         new_data->opponent_grid=&game->grid1;
                         opponent_ships=&game->ships1;
                         notopponent_grid=&game->grid2;
                         notopponent_ships=&game->ships2;
                         new_data->target_fd=player1;
                         not_target=player2;
                }
                else if(target==2){
                         current_grid=&game->grid3;
                         current_ships=&game->ships3;
                         new_data->opponent_grid=&game->grid2;
                         opponent_ships=&game->ships2;
                         notopponent_grid=&game->grid1;
                         notopponent_ships=&game->ships1;
                         new_data->target_fd=player2;
                         not_target=player1;
                }
     }
	snprintf(new_data->coordinates, sizeof(new_data->coordinates), "%d %d",new_data->x,new_data->y);
   
    signal18:  //signal a opponent per inviare le coordinate
    oper.sem_num = 2;
    oper.sem_op = 1;
    oper.sem_flg = 0;
    new_data->ret  = semop(new_data->sd,&oper,1);
    if ( new_data->ret== -1 && errno == EINTR){
        goto signal18;
    }

        
    wait17: //wait
    oper.sem_num = 0;
    oper.sem_op = -1;
    oper.sem_flg = 0;
    new_data->ret = semop(new_data->sd,&oper,1);
    if (new_data->ret== -1 && errno == EINTR){
        goto wait17;
    }

    if ((*new_data->opponent_grid)[new_data->x][new_data->y] == 1) {
            (*new_data->opponent_grid)[new_data->x][new_data->y] = 3; // Colpito
            (*opponent_ships)--;
    }
    else if((*new_data->opponent_grid)[new_data->x][new_data->y]==0) {
            (*new_data->opponent_grid)[new_data->x][new_data->y] = 2; // Mancato
    }
     
    signal19:    //signal a current
    oper.sem_num = 1;
    oper.sem_op = 1;
    oper.sem_flg = 0;
    new_data->ret = semop(new_data->sd,&oper,1);
    if (new_data->ret == -1 && errno == EINTR){
		  goto signal19;
    }
     
    wait18:   //wait
    oper.sem_num = 0;
    oper.sem_op = -1;
    oper.sem_flg = 0;
    new_data->ret  = semop(new_data->sd,&oper,1);
    if ( new_data->ret== -1 && errno == EINTR){
        goto wait18;
    }

    if(new_data->t==1){
        if(send_data(not_target, "1")==false){
		break;
      	} 
		new_data->t=0;
    }
    else if(new_data->t==3){
        if(send_data(not_target, "3")==false){
			break;
	    } 
        new_data->t=0;
    }

    if (check_elimination(*opponent_ships)) {
        printf("Il giocatore colpito %d ha abbandonato la partita in quanto sono state eliminate tutte le sue navi\n", new_data->target_fd);
        new_data->terminate_flag = 1;
        close(new_data->target_fd);
        if (semctl(new_data->sd, 0, IPC_RMID) == -1) {
           perror("Errore nella rimozione del semaforo");
           exit(-1);
        }  
        twoplayers(new_data->current_player,not_target,current_grid,current_ships,notopponent_grid,notopponent_ships);
        free(new_data);   
    }

    int temp = new_data->current_player;
    new_data->current_player = new_data->other_player;
    new_data->other_player = new_data->other_player2;
    new_data->other_player2 = temp;
 }

 close(player1);
 close(player2);
 close(player3);
 free(new_data);
 printf("Effettuata la free di new_data in game_handler\n");
 free(game);
 printf("Effettuata la free di game in game_handler\n");
 pthread_exit(NULL);
}

void offgame(){
		close(server_socket);
}

void offsignal(){
        offgame();
        printf("\nIl server e' stato chiuso correttamente \n");
        exit(-1);
}

int funcsignal(){
        sigset_t mask;
        struct sigaction act;
        act.sa_handler=offsignal;
        act.sa_flags=0;
        sigfillset(&mask);
        sigdelset(&mask,SIGINT);
        sigdelset(&mask,SIGTERM);
        if(sigprocmask(SIG_SETMASK,&mask,NULL)==-1){
                printf("Errore nel creare maschera di segnali\n");
                exit(-1);
        }
        if(sigaction(SIGINT,&act,NULL)==-1){
                printf("Errore nel settare sigint\n");
                exit(-1);
        }
        if(sigaction(SIGTERM,&act,NULL)==-1){
                printf("Errore nel settare sigterm\n");
				 exit(-1);
        }
        return 0;
}

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

int ipcontrol(char *ip){
        //funzione che controlla la validità dell'indirizzo ip ip, mediante la funzione inet_pton
        struct sockaddr_in sa;
        if(inet_pton(AF_INET,ip,&(sa.sin_addr))!=1){
                return 1;
        }
        return 0;
}

int initSocket(char *port, char *ipAddress){
        int ret;
        struct sockaddr_in address;
        if(ret=portvalidate(port)!=0){
                if(ret==1){
                        printf("Non è valida la porta %s\n",port);
                        return 1;
                }
                if(ret==2){
                        printf("La porta %s ha valore minore di 5001\n",port);
                        return 1;
                }
                if(ret==3){
					    printf("La porta %s ha valore maggiore di 65535\n",port);
                        return 1;
                }
        }
        int porta=strtol(port,NULL,10);
        if(ret=ipcontrol(ipAddress)!=0){
                if(strcmp(ipAddress,"null") != 0){                                                                                                                            printf("l'indirizzo ip %s non e' corretto\n",ipAddress);
                  return 1;
                }
        }
        if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
           perror("Socket failed");
           exit(EXIT_FAILURE);
        }
        printf("server_socket e' uguale a %d\n",server_socket);

        address.sin_family = AF_INET;
        if(ret!=0){
                address.sin_addr.s_addr=INADDR_ANY;
        }
        else{
                  address.sin_addr.s_addr=inet_addr(ipAddress);
        }
        address.sin_port = htons(porta);

        if (bind(server_socket, (struct sockaddr *)&address, sizeof(address)) ==-1) {
          perror("Bind failed");
          return 1;
        }

        if (listen(server_socket, 3) < 0) {
          perror("Listen");
          exit(EXIT_FAILURE);
        }
        printf("il server si e' connesso alla porta %d\n",porta);
        return 0;
}

int main(int argc,char **argv){
    int new_socket;
    int ret;
    pthread_t thread_id;
    if(argc!=3){
                printf("Non sono stati inseriti abbastanza parametri\n");
                exit(-1);
    }
    if(initSocket(argv[1],argv[2]) != 0){
        printf("Errore: non è possibile connettersi al server\n");
        exit(-1);
    }

    signal(SIGPIPE, SIG_IGN);   
    
    if(funcsignal()!=0){
        printf("Errore nella gestione dei segnali\n");
        exit(-1);
    }
    struct sockaddr_in clientaddr[3];
    socklen_t len[3];

    len[0] = sizeof(clientaddr[0]);
    len[1] = sizeof(clientaddr[1]);
    len[2] = sizeof(clientaddr[2]);

    int games = 0;
    while (1) {
        Game *new_game = malloc(sizeof(Game));
        if (!new_game) {
            perror("Malloc failed");
            continue;
        }

        printf("In attesa di giocatori...\n");
        accept1:
		new_game->player1_socket = accept(server_socket, (struct sockaddr *)&clientaddr[0], (socklen_t*)&len[0]);
        if (new_game->player1_socket < 0) {
            perror("Player 1 accept failed");
            goto accept1;
        }
		printf("Giocatore 1 connesso.\n");
        numclient++;

        accept2:
        new_game->player2_socket = accept(server_socket, (struct sockaddr *)&clientaddr[1], (socklen_t*)&len[1]);
        if (new_game->player2_socket < 0) {
            perror("Player 2 accept failed");
            close(new_game->player2_socket);
            //free(new_game);
            goto accept2;
        }
        printf("Giocatore 2 connesso.\n");
        numclient++;
        
		accept3:
        new_game->player3_socket = accept(server_socket, (struct sockaddr *)&clientaddr[2], (socklen_t*)&len[2]);
        if (new_game->player3_socket < 0) {
            perror("Player 3 accept failed");
            close(new_game->player3_socket);
            //free(new_game);
            goto accept3;
        }
        printf("Giocatore 3 connesso.\n");
        numclient++;

        games+=1;
        new_game->game_id = games;
		
        // Crea un nuovo thread per la partita
        ret=pthread_create(&thread_id, NULL, game_handler, (void*)new_game);
        if(ret!=0){
                printf("errore nella creazione del thread\n");
				free(new_game);
                exit(-1);
        }
        pthread_detach(thread_id);
    }
    return 0;
}
