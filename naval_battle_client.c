#include <stdio.h>										//gestore dei file
#include <stdlib.h>										//gestore memoria dinamica
#include <string.h>										//gestore stringhe
#include <sys/stat.h>		//contiene la struttura stat che serve per le statistiche del file come la st_size
#include <unistd.h>	
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>								// serve per avere la struttura sockaddr_in e struct in_addr
#include <errno.h>
#include <sys/time.h>								//gestore tempo
												
#define SA struct sockaddr
#define LUN 10										//grandezza mappa
#define max_buf 30									//grandezza buffer

//------- variabili -------//
const char* s_host_remoto;							//primo argomento passato al main
const char* s_porta_remota;  						//secondo argomento passato al main
int porta_remota;					//porta remota server,convertita da stringa ad intero con la atoi(string*)

int lun;											//lunghezza parametri
char* comando;										//comando
char* parametro;									//eventuale parametro dei comandi
char* par, *para;									//parametri
char* buffer;										//buffer
char buf[max_buf];
int check;											//controllo funzioni
/*nella conversione della porta, punta al primo carattere successivo alla porzione di stringa che è stata convertita*/
char* endptr; 							

char* err= "[ERROR] ";								//variabile che serve per l'errore
char coord[max_buf];								//ingresso coordinate
char orient[max_buf];								//ingreso orientamento

int mappa[LUN][LUN];								//copia della mappa di gioco 
int orig_mappa[LUN][LUN];							//mappa di gioco originale
char* mio_nome;										//nome scelto
char* mio_indirizzo;								//indirizzo IP
int mia_porta;										//porta di ascolto

int sf_mappa[LUN][LUN];								//mappa client sfidato
char* sfidato_nome;									//nome client sfidato
char* sfidato_indirizzo;							//indirizzo IP client sfidato
int sfidato_porta;									//porta di ascolto client sfidato

/*timeval - specifica un periodo di timeout. Se il tempo viene superato e select() non ha trovato nessun descrittore di file pronto in lettura, essa termina*/
struct timeval tv;

int fdmax;											//massimo numero di descrittori di  file 
int yes = 1;										//per setsockopt() SO_REUSEADDR
fd_set read_fds;								// lista dei secrittori di file temporanea per  select() (temp)
/*master - lista dei descrittori di file principale (master) contiene tutti i descrittori di file che sono attualmente connessi, e anche il descrittore che è in attesa di nuove connessioni.*/
fd_set master;								

int sck;											//socket per la connessione TCP con il client
struct sockaddr_in srv_addr;						//struttura dati contenente l'indirizzo del server
struct sockaddr_in client_addr;						//struttura dati contenente l'indirizzo del client

int sck_UDP;										//socket per la connessione UDP 
struct sockaddr_in UDP_addr;						//struttura dati per la connessione UDP
socklen_t len_UDP_addr;								//variabile

int turno;			//avviata la partita indica il turno dei giocatori: proprio turno (1), turno avversario (0)
int in_partita;										//indica se si sta giocando una partita (1) o no (0)
int colpito;										//numero di punti da centrare per affondare tuttte le navi
		
int i, j, k, vero, i_for, x, y, n_client, var;		//variabili 
int check1, check2, rig, col, x1, x2, y_1, y2;

//------- funzioni -------//
int inserisci_comandi();
void in_coord(int k, int h);
void in_orien(int k, int h);
void costruisci_mappa(int j, int var);
void stampa_mappa(int map[LUN][LUN]);
char controlla_mappa(char j, char k);
void server_UDP();
void client_UDP();
void disconnect(int mot);


int main(int n_arg, char** arg)
{
	//------- inizializzazioni -------//
	//controllo che gli argomenti passati al main siano tutti 	
	if (n_arg!=3){
		printf("%sThe right syntax to start naval_battle_client is: ",err); 			
		printf("./naval_battle_client <remote host> <port>\n\n");
		exit(1);
	}					

	s_host_remoto= arg[1];							//indirizzo IP del server per connessione TCP
	s_porta_remota= arg[2];							//porta remota sulla quale ascolta il server
	porta_remota= atoi(s_porta_remota);	//conversione della stringa che identificava la porta remota in numero
	if (porta_remota <1024 || porta_remota>65535){
			//controlla se la porta ha superato il range
			printf("%sThe remote port must be in the range [1024, 65535]. \n\n", err);
			exit(1);			
	}

	/*sck_UDP - lo inizializzo con un numero casuale altrimenti la select() lo legge come inizializzato a 0 (input) prima di iniziare una partita*/
	sck_UDP=150;	
	in_partita=0;									//non siamo in partita

	FD_ZERO(&master);						//inizializza l’insieme di descrittori di master con l’insieme vuoto
	FD_ZERO(&read_fds);					//inizializza l’insieme di descrittori di read_fds con l’insieme vuoto
	tv.tv_sec=600;									//impostiamo 60 secondi per la select
	tv.tv_usec=0;
		
	//creazione socket TCP, come da specifica, per la connessione al server
	sck = socket(PF_INET, SOCK_STREAM, 0);
	if(sck==-1){
		printf("%sFailed to crate the socket.\n\n", err);
		exit(1);			
	}

	memset(&srv_addr, 0, sizeof(srv_addr));			//azzeramento struttura indirizzo server
	srv_addr.sin_family= AF_INET;
	//htons - host to network, conversione del numero della porta da formato locale a formato di rete
	srv_addr.sin_port = htons(porta_remota); 				//porta connessione al server

	//------- connect -------//
	/*inet_pton - trasforma s_host_remoto da formato presentation (127.0.0.1) a formato numeric (00011101010101010) e lo memorizza in srv_addr.sin_addr*/
	check = inet_pton(AF_INET, s_host_remoto, &srv_addr.sin_addr);
	if(check==0){
		printf("%sThe host address must be 4 numbers in the range [0-255] separated by dots.\n", err);
		printf("es. 127.0.0.1\n\n");
		exit(1);
	} 

	//connect - stabilisce la connessione con il server
	check = connect(sck, (SA*) &srv_addr, sizeof(srv_addr));
	if(check==-1)	{
		printf("%sConnection failed.\n", err);
		printf("Check the port and the address of the remote server.\n\n");
		exit(1);	
	}
	
	FD_SET(sck, &master);	//aggiunge sck all’insieme di descrittori master (mette ad 1 il bit relativo a sck)
	FD_SET(0, &master);					//aggiunge lo standard input (0) all'insieme dei descrittori di master
 
	fdmax= sck; 						//tiene traccia del valore massimo dei descrittori in uso finora è sck

	//------- inizializzazione client -------//
	//prima connessione riceviamo la disponibilità del server (restituira' -1 se non è andata a buon fine)
	check = recv(sck, (void*) &lun, 4, 0);   
	if(check==-1 || check<4){
		printf("%sServer connection failed.\n", err);
		exit(1);
	}
	//lun - risposta di capacità nel server 
	if(!lun){
		printf("%sMaximum server capacity reached. \nTry again later. \n", err);
		return 0;
	}
	
	//------- connessione stabilita -------//
	//ricezione mio indirizzo ip (lun+IP)
	check=recv(sck, (void*)&lun,4,MSG_WAITALL);		
	if((check==-1) || (check<4)) {
		printf("%sFailed to receive yor IP length.\n",err);
		exit(1);
	}
	mio_indirizzo= malloc(lun+1);		
	check= recv(sck, (void*)mio_indirizzo, lun, 0);					
	if((check==-1) || (check<4)) {
		printf("%sFailed to receive your IP.\n",err);
		exit(1);
	}
	mio_indirizzo[lun] = '\0';
	
	//il server è disponibile, avvio interfaccia utente
	lun=strlen("clear");	
	para=malloc(lun+1);
	strcpy(para, "clear");
	para[lun]='\0';
	system(para);										//pulizia terminale
	free(para);
	
	printf("Server connection %s (port %d) success!", s_host_remoto, porta_remota);
	printf("\n\n");	
	
	//comandi disponibili
	printf("Commands:\n");
	printf("* !help --> shows the commands list\n");
	printf("* !who --> shows the list of the clients connetted with the server\n");
	printf("* !connect client_name --> starts a game with the client client_name\n");
	printf("* !disconnect --> closes the game with the other client\n");
	printf("* !quit --> disconnects from the server\n");
	printf("* !show-enemy-map --> shows the enemy map\n");
	printf("* !show-my-map --> shows my map\n");
	printf("* !hit coordinates --> hits the coorrdinates 'coordinates' (valid only when is my turn to play)");
	printf("\n\n");

	buffer=malloc(max_buf);
	par=malloc(max_buf);
	//------- invio nome -------//	
	do{
		var=1;
		do{
			printf("\nInsert your name: ");
			fflush(stdin);
			memset(buffer, '\0', max_buf);				//caratteri di fine stringa, azzeramento struttura
			fgets(buffer, max_buf, stdin);				//prelievo caratteri dallo stdin
			buffer[strlen(buffer)-1]= '\0';				//carattere di fine stringa
		}while (strlen(buffer)==0);						//cicla finchè non si inserisce il nome
			
		par= strtok(buffer, " ");						//prende la parola prima incontrata dopo uno spazio
		
		//invio nome del client al server - (lun+ nome)
		lun= strlen(par);
		check = send(sck, (void*) &lun, 4, MSG_WAITALL);	
		if((check==-1)||check<4){
			printf("%sFailed to send name length.\n",err);
			exit(1);
		}
		check = send(sck, (void*)par, lun, 0);				
		if(check==-1 || check <lun){
			printf("%sFailed to send the name.\n",err);
			exit(1);
		}

		//risposta server nome già esistente
		check = recv(sck, (void*) &var, 4, 0);   
		if(check==-1 || check<4){
			printf("%sFailed to receive name list.\n", err);
			exit(1);
		}	
		
		if(!var)
			printf("%sName already in use, please type another name.\n", err);

	}while(!var);										//cicla finchè non si invia un nome valido al server 

	mio_nome=malloc(strlen(par)+1);						//memorizzazione proprio ome
	strcpy(mio_nome, par);
	mio_nome[strlen(par)]='\0';

	//------- invio porta UDP -------//
	do{	
		vero=0;
		printf("Please, insert the listener UDP port: ");
		fflush(stdin);
		memset(buffer, '\0', max_buf);					//caratteri di fine stringa, azzeramento struttura
		fgets(buffer, max_buf, stdin);					//prelievo caratteri dallo stdin
		buffer[strlen(buffer)-1]= '\0';					//carattere di fine stringa
			
		if(strlen(buffer)!=0){							//se buffer non vuoto
			par= strtok(buffer, " ");					//prende la parola prima incontrata dopo uno spazio

			mia_porta= atoi(par);		//conversione della stringa che identificava la porta remota in numero
			if(mia_porta<1024 || mia_porta>65535){		//controllo correttezza porta
				printf("%sThe port must be in the range [1024, 65535] \n\n",err);
				vero=1;
			}
		}
	}while ((strlen(buffer)==0) || (vero==1));			//ciclo finchè non viene inserita una porta valida
	//par -porta in formato dotted decimal

	//invio porta del client al server (lun+ porta)
	lun= strlen(par);
	check = send(sck, (void*) &lun, 4, MSG_WAITALL);
	if((check==-1)||check<4){
		printf("%sFailed to send UDP port lenght.\n",err);
		exit(1);
	}
	check = send(sck, (void*)par, lun, 0);					
	if(check==-1 || check <lun){
		printf("%sFailed to send UDP port.\n",err);
		exit(1);
	}

	//------- immissione coordinate -------//
	j=6, var=1;
	for(k=0; k<4; k++){	
		//for(i=0; i<(k+1); i++){
	 		in_coord(var, j);
			in_orien(var, j);		
			costruisci_mappa(j, var);
			var++;
		//}
		if(k==0)
			j=j-2;
		else
			j--;
	}

	//il client è stato inizializzato correttamente
	printf(">");
	fflush(stdout);

	while(1){
		//------- select -------//
		read_fds = master;								//copia master in read_fds
		check=select(fdmax+1, &read_fds, NULL, NULL, &tv);
		if(check == -1){ 
			printf("%sFailed to execute select() method.\n",err); 
			exit(1); 
		}

		if(check){
			//cicla tra le connessioni esistenti in attesa di dati da leggere
			for(i_for = 0; i_for <= fdmax; i_for++){
				/*FD_ISSET - al ritorno di select(), controlla se i_sel appartiene all’insieme di descrittori read_fds, verificando se il bit relativo a i_for è pari a 1*/
				if(FD_ISSET(i_for, &read_fds)){
					if(i_for==0){
						//------- stdin -------//
						//il client ha digitato caratteri sullo stdin
						//eseguo il comando
						check=inserisci_comandi();
			
						if(!check){					//il client ha eseguito !quit e si esce dal main
							return 0;
						}
						
					}
					if(i_for==sck_UDP){
						//------- socket UDP -------//
						//l'avversario ha inviato un comando
						memset(buf, '\0', max_buf);			//azzeramento struttura buf		
						check= recvfrom(sck_UDP, buf, max_buf-1, 0, (SA*)&UDP_addr, &len_UDP_addr);
						if((check==-1) || (check<4)) {
							printf("%sFailed to receive the command.\n",err);
							exit(1);
						}
						buf[check]=' ';
					
						//------- comando !hit -------//
						if(strcmp(buf, "!hit")==0){
							//ricezione coordinate
							memset(buf, '\0', max_buf);		
							check= recvfrom(sck_UDP, buf, max_buf-1, 0, (SA*)&UDP_addr, &len_UDP_addr);
							if((check==-1) || (check<4)) {
								printf("%sFailed to receive the coordinates.\n",err);
								exit(1);
							}
							buf[check]=' ';

							printf("\n%s torpedoed in coordinates %c%c: ", sfidato_nome,buf[0], buf[1]);	

							//verifica mappa e aggiornamento
							buf[0]=controlla_mappa(buf[0], buf[1]);
					
							//invio risultato attacco (cosidero solo buf[0])
							check= sendto(sck_UDP, buf, max_buf-1, 0, (SA*)&UDP_addr, sizeof(UDP_addr));
							if((check==-1) || (check<4)) {
								printf("%sFailed to send hitted boat result.\n",err);
								exit(1);		
							}

							//cambio turno
							if(turno)
								turno=0;
							else
								turno=1;

							printf("It's your turn:\n");
							printf("\n#");
							fflush(stdout);
						}

						//------- comando !disconnect -------//
						if(strcmp(buf, "!disconnect")==0){
							check = close(sck_UDP);			//chiusura socket UDP
							if(check!=0){
								printf("%sFailed to close socket_UDP.",err);
								exit(1);			
							}
	
							FD_CLR(sck_UDP, &master);		//rimozione socket da master set
						
							in_partita=0;					//non più in partita

							printf("\n%s closed the game: YOU WIN!!\n", sfidato_nome);
							printf(">");
							fflush(stdout);	
						}	
					}
					if(i_for == sck){
						//------- socket TCP -------//
						//ricezione nome sfidante - (lun+nome)
						check = recv(sck, (void*) &lun, 4,MSG_WAITALL);   
						if(check==-1 || check<4){
							printf("%sFailed to receive enemy name lenght.\n", err);
							exit(1);
						}
						par= realloc(NULL, lun+1);		
						check= recv(sck, (void*)par, lun, 0);					
						if((check==-1) || (check<4)) {
							printf("%sFailed to receive enemy name.\n",err);
							exit(1);
						}
						par[lun] = '\0';
						//par - nome sfidante		
	
						printf("\nThe peer %s want play a game with you, do you want to play?", par);
			
						buffer=realloc(NULL, max_buf);
			
						//risposta
						do{
							do{
								printf("\nPress S to accept, N to decline\n");
								printf(">");
								fflush(stdout);
								memset(buffer, '\0', max_buf);				//azzeramento struttura
								fflush(stdin);
								fgets(buffer, max_buf, stdin);				//prelievo caratteri dallo stdin
								buffer[strlen(buffer)-1]= '\0';				//carattere di fine stringa
							}while (strlen(buffer)==0);				//cicla finchè non si inserisce S o N
						}while((buffer[0]!='S' && buffer[0]!='N') || buffer[1]!='\0');
	
						//se rispondo S accetto la partita, altrimenti la rifiuto
						//invio al server lun=1 accettata - lun=0 rifiutata
						if(buffer[0]=='S'){				//partita accettata
							printf("\nYour new enemy is %s", par);

							//invio risposta positiva al server							
							lun=1;
							check = send(sck, (void*) &lun, 4, MSG_WAITALL);
							if((check==-1)||check<4){
								printf("%sFailed to send positive response.\n",err);
								exit(1);
							}

							sfidato_nome=malloc(strlen(par)+1); //memorizzazione nome sfidante
							strcpy(sfidato_nome, par);
					
							//azzeramento mappa avversario - inizialmente non si sono eseguiti attacchi
							for(j = 0; j < LUN; j++)
								for(k = 0; k < LUN; k++)
			  						sf_mappa[j][k]=0;

							//copio mappa da orig_mappa a mappa in modo da giocare su una copia
							for(j = 0; j < LUN; j++)
								for(k = 0; k < LUN; k++)
	  								mappa[j][k]=orig_mappa[j][k];
					
							colpito= 31; 						//posizioni da indovinare per vincere

							//------- inizio partita -------//
							//connessione come server UDP
							server_UDP();
						}						
						else{							//partita rifiutata
							printf("\nYou declined the game with %s.\n", par);
							printf(">");
							fflush(stdout);
						
							//invio risposta negativa al server							
							lun=0;
							check = send(sck, (void*) &lun, 4, MSG_WAITALL);
							if((check==-1)||check<4){
								printf("%sFailed to sent the declined response.\n",err);
								exit(1);
							}

							//ritorno alla select()
						}						
					}
				
				}
			}
		}
		else{ 											//è scattato il timeout	
			if(in_partita){								//se si sta giocando una partita
			printf("%sTimeout - notight received from the UDP socket or from the stdin", err);
			printf(" for more than ten minutes\n");							
			disconnect(2);								//disconnessione come da specifica
			}
		}
	}
	free(comando);
	free(parametro);
	free(buffer);
	free(par);
	free(mio_indirizzo);
	free(mio_nome);
	return 0;
}

//------- client UDP -------//
void client_UDP()
{
	//------- inizializzazioni -------//
	memset(&UDP_addr, 0, sizeof(UDP_addr));				//azzeramento struttura dati indirizzo UDP
	UDP_addr.sin_family = AF_INET;
	UDP_addr.sin_port = htons(sfidato_porta);			//porta client sfidato

	//inet_pton - passa da formato presentation (127.0.0.1) a formato numeric (00011101010101010)
	check = inet_pton(AF_INET, sfidato_indirizzo, &UDP_addr.sin_addr); //indirizzo clien sfidato
	if(check==0){
		printf("%sFailed to convert enemy IP address for UDP socket\n\n", err);
		exit(1);
	} 	
	
	len_UDP_addr = sizeof(UDP_addr);

	//------- socket -------//
	//socket UDP come da specifica
	sck_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	if (sck_UDP==-1){
			printf("%sFailed to create UDP client socket.\n\n",err);
			exit(1);
	}

	FD_SET(sck_UDP, &master);							//aggiunge sck_UDP all’insieme di descrittori master
	if(sck_UDP > fdmax)									//tiene traccia del più grande descrittore 
		fdmax = sck_UDP;

	in_partita=1;										//si sta giocando una partita
	turno=1;											//proprio turno

	fflush(stdin);										//svuotamento stdin per eventuali caratteri spuri
	printf("It's your turn: ");
	printf("\n#");
	fflush(stdout);

	return;
}

//------- server UDP -------//
void server_UDP()
{
	//------- inizializzazioni -------//
	memset(&UDP_addr, 0, sizeof(UDP_addr));				//azzero struttura UDP_addr
	UDP_addr.sin_family = AF_INET;						
	UDP_addr.sin_port = htons(mia_porta);
	
	//inet_pton - passa da formato presentation (127.0.0.1) a formato numeric (00011101010101010)
	check = inet_pton(AF_INET, mio_indirizzo, &UDP_addr.sin_addr);
	if(check==0){
		printf("%sFailed to convert your IP address for UDP socket.\n\n", err);
		exit(1);
	} 

	len_UDP_addr = sizeof(UDP_addr);

	//------- socket -------//
	//socket UDP come da specifica
	sck_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	if (sck_UDP==-1){
			printf("%sFailed to create UDP server socket.\n\n",err);
			exit(1);
	}

	//------- bind -------//
	check=bind(sck_UDP,(SA*) &UDP_addr, sizeof(UDP_addr));
	if (check == -1){
		printf("%sFailed to bind UDP server.\n\n",err);
		exit(1);
	}

	FD_SET(sck_UDP, &master);							//aggiunge sck_UDP all’insieme di descrittori master
	if(sck_UDP > fdmax)									//tiene traccia del più grande descrittore 
		fdmax = sck_UDP;

	in_partita=1;										//si sta giocando una partita
	turno=0;											//turno avversario

	printf("\nIt's %s's turn", sfidato_nome);
	printf("\n#");
	fflush(stdout);

	return;
}
			
//------- controlla mappa -------//
char controlla_mappa(char j, char k)
{
	//leggenda
	//0= vuoto
	//1= nave
	//2= colpito
	//3= acqua

	//conversione coordinate da caratteri a interi
	x= j-65;											//coordinata x (colonna)
	y= k-48;											//coordinata y (riga)
	
	//controllo 
	if (mappa[y][x]==1){								//nave colpita
		printf("HITTED\n");
		mappa[y][x]=2;
		colpito--;
		if(colpito==0)
			disconnect(3);								//l'avversario ha vinto la partita
		return 'c';		
	}
	if (mappa[y][x]==0){								//acqua
		printf("WATER\n");
		mappa[y][x]=3;
		return 'a';
	}
	if (mappa[y][x]==2){								//nave già colpita
		printf("Boat already hitted\n");
		return 's';
	}
	if (mappa[y][x]==3){								//già trovata acqua
		printf("Water already founded\n");
		return 't';
	}

	return 'e';											//errore
}

//------- comandi disponibili -------//
int inserisci_comandi()
{
	buffer=realloc(NULL, max_buf);
	comando=malloc(max_buf);
	parametro=malloc(max_buf);

	//prelievo del comando dal buffer stdin
	memset(buffer,'\0', max_buf);						//azzeramento buffer
	fgets(buffer, max_buf, stdin);						//prelievo dallo stdin
	buffer[strlen(buffer)-1]= '\0'; 					//carattere di fine stringa

	if (strlen(buffer)==0){								//nessun carattere inserito
		if(in_partita){
			printf("\n#");
			fflush(stdout);
		}
		else{
			printf(">");
			fflush(stdout);
		}
		return 1;						//nessun comando inserito - ritorno alla select()
	}

	//prelievo il comando e l'eventuale parametro 
	comando= strtok(buffer, " ");			//prende la parola prima di incontrare uno spazio
	if (comando==NULL){							//inseriti solo spazi - ritorno alla select()
		if(in_partita){
			printf("\n#");
			fflush(stdout);	
		}
		else{
			printf(">");
			fflush(stdout);	
		}
		return 1;	
	}													

	parametro= strtok(NULL, " ");						//prendo il parametro (se c'è)
					
	//------- comando !help -------//
	if(strcmp(comando, "!help")==0){			
	printf("* !help --> shows the commands list\n");
	printf("* !who --> shows the list of the clients connetted with the server\n");
	printf("* !connect client_name --> starts a game with the client client_name\n");
	printf("* !disconnect --> closes the game with the other client\n");
	printf("* !quit --> disconnects from server\n");
	printf("* !show-enemy-map --> shows the enemy map\n");
	printf("* !show-my-map --> shows my map\n");
	printf("* !hit coordinates --> hits the coorrdinates 'coordinates' (valid only when is my turn to play)");

		if(in_partita){
			printf("\n#");
			fflush(stdout);	
		}
		else{
			printf(">");
			fflush(stdout);	
		}

		return 1;															
	}	

	//------- comando !who -------//	
	if( strcmp(comando, "!who")==0){
		//invio comando al server (lun+comando)
		lun= strlen(comando);
		check = send(sck, (void*) &lun, 4, MSG_WAITALL);
		if((check==-1)||check<4){
			printf("%sFailed to send command length.\n",err);
			exit(1);
		}
		check = send(sck, (void*)comando, lun, 0);
		if(check==-1 || check <lun){
			printf("%sFailed to send command.\n",err);
			exit(1);
		}
	
		//ricezione dal server del numero di client connessi al server
		check=recv(sck, (void*) &n_client ,4,MSG_WAITALL);
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive client number.\n",err);
			exit(1);				
		}
		//n_client contiene il numero dei client connessi
		printf("Clients connected: ");
	
		//ricezione e stampa client connessi al server
		for(i=0; i<n_client; i++){
			//ricezione nome (lun+nome)
			check=recv(sck, (void*)&lun,4,MSG_WAITALL);			
			if((check==-1) || (check<4)) {
				printf("%sFailed to receive peer length name.\n",err);
				exit(1);
			}	
			par= realloc(NULL,lun+1);							//occupazione memeria dinamica
			check = recv (sck, (void*)par, lun, 0);		
			if((check==-1) || (check<4)) {
				printf("%sFailed to receive peer name.\n",err);
				exit(1);
			}
			par[lun] = '\0';							//carattere di fine stringa
			printf("%s ", par);							//stampa nome client connesso
		}
		//ricevuti tutti i nomi dei client connessi
		
		if(in_partita){
			printf("\n#");
			fflush(stdout);	
		}
		else{
			printf("\n>");
			fflush(stdout);	
		}

		return 1;
	}

	//------- comando !connect nome_client -------//
	if(strcmp(comando,"!connect") == 0){
		if(in_partita){
			//si sta svolgendo una partita, per eseguire questo comando bisogna prima disconnettersi
			printf("%sYou are playing a game. ",err);
			printf("\nSend !disconnect to stop the current game.\n");
			printf("\n#");
			fflush(stdout);
			return 1;
		}
		
		if(parametro==NULL){
			//nessun nome client da sfidare specificato										
			printf("%s client_name not specified.\n",err);
			printf(">");
			fflush(stdout);
			return 1;
		}
		
		//nome client da sfidare troppo lungo
		if(strlen(parametro)>max_buf){								
			printf("%sThe client_name that you type is too long.\n",err);
			printf("Please insert max 30 charatters.\n");
			printf(">");
			fflush(stdout);
			return 1;
		}
		
		//si è inserito il proprio nome
		if(strcmp(mio_nome, parametro)==0){						
			printf("%sInvalid client name, is not possible to play a game with yourself.\n",err);
			printf("Please, insert another client name.\n");
			printf(">");
			fflush(stdout);
			return 1;
		}
		
		//invio comando al server (lun+comando)	
		lun= strlen(comando);
		check = send(sck, (void*) &lun, 4, MSG_WAITALL);
		if((check==-1)||check<4){
			printf("%sFailed to send command length.\n",err);
			exit(1);
		}
		check = send(sck, (void*)comando, lun, 0);
		if(check==-1 || check <lun){
			printf("%sFailed to send command.\n",err);
			exit(1);
		}
	
		//invio nome client da sfidare al server (lun + nome)
		lun = strlen(parametro);
		check= send(sck, (void*)&lun, 4, MSG_WAITALL);
		if(check==-1 || check <4){
			printf("%sFailed to send enemy length name.\n",err);
			exit(1);
		}
		check = send(sck, (void*)parametro, lun, 0);
		if(check==-1||check<lun){
			printf("%sFailed to send enemy name.\n",err);
			exit(1);
		}

		//ricezione nome del client sfidato(anche se lo conosco) o errore (lun+nome)
		check=recv(sck, (void*)&lun,4,MSG_WAITALL);		
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive name length.\n",err);
			exit(1);
		}
		par= realloc(NULL,lun+1);		
		check= recv(sck, (void*)par, lun, 0);					
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive name.\n",err);
			exit(1);
		}
		par[lun] = '\0';	
		//par - contiene nome client sfidato(=parametro) o la stringa errore	
	
		//client non esistente sul server
		if(strcmp(par, "error") == 0){
			printf("Connection with %s failed: user doesn't exist.\n", parametro);
			printf(">");
			fflush(stdout);
			return 1;
		}
		
		//client esistente
		//il server controlla se il client sfidato è occupato e invia la risposta
		check=recv(sck, (void*)&lun,4,MSG_WAITALL);				
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive status client.\n",err);
			exit(1);
		}

		//risposta client occupato: lun=0 client occupato; lun=1 client libero
		if(!lun){										//client occupato in un'altra partita - esco
			printf("Connection with %s failed: the client is already busy with another game.\n\n", par);
			printf(">");
			fflush(stdout);
			return 1;
		}

		//client sfidato esistente e libero 
		//il server chiede al client sfidato se vuole giocare e invia la risposta
		check=recv(sck, (void*)&lun,4,MSG_WAITALL);	
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive answer.\n",err);
			exit(1);
		}
				
		//risposta del client sfidato data al server
		//lun=0 partita rifiutata - lun=1 partita accettata
		if(!lun){										//il client sfidato non ha accettato la partita - esco
			printf("Connection with %s failed: the client decline your challenge.\n\n", par);
			printf(">");
			fflush(stdout);			
			return 1;
		}

		//il client sfidato ha accettato la partita 
		printf("%s accepted your challenge.\n", par);

		//ricezione (dal server) porta di ascolto del client sfidato
		check=recv(sck, (void*)&lun,4,MSG_WAITALL);
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive UDP port.\n",err);
			exit(1);
		}					
					
		sfidato_porta=lun;								//memorizza porta client sfidato
		sfidato_nome=malloc(strlen(par)+1);
		strcpy(sfidato_nome, par);						//memorizza nome client sfidato
		sfidato_nome[strlen(par)]='\0';

		//ricezione (dal server) indirizzo IP del client sfidato (lun+IP)
		check=recv(sck, (void*)&lun,4,MSG_WAITALL);		
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive IP address length.\n",err);
			exit(1);
		}
		par= realloc(NULL,lun+1);		
		check= recv(sck, (void*)par, lun, 0);					
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive IP address.\n",err);
			exit(1);
		}
		par[lun] = '\0';
						
		sfidato_indirizzo=malloc(strlen(par)+1);		//memorizza indirizzo IP client sfidato
		strcpy(sfidato_indirizzo, par);
		sfidato_indirizzo[strlen(par)]='\0';
	
		//azzeramento mappa avversario - inizialmente non è stato eseguito nessun attacco
		for(j = 0; j < LUN; j++)
			for(k = 0; k < LUN; k++)
				sf_mappa[j][k]=0;

		//copio mappa da orig_mappa a mappa in modo da giocare su una copia
		for(j = 0; j < LUN; j++)
			for(k = 0; k < LUN; k++)
				mappa[j][k]=orig_mappa[j][k];

		colpito= 31;									//posizioni da indovinare per vincere

		//------- inizio partita -------//
		//avvio partita su un socket UDP
		printf("Game started with %s\n\n", sfidato_nome);

		//connessione come client UDP
		client_UDP();
				
		return 1;		
	}

	//------- comando !show-my-map -------//
	if(strcmp(comando,"!show-my-map") == 0){
		printf("\n");
		if(in_partita){
			stampa_mappa(mappa);						//mappa modificata nella partita
			printf("\n#");
			fflush(stdout);
		}
		else{
			stampa_mappa(orig_mappa);					//mappa originale
			printf("\n>");
			fflush(stdout);
		}

		return 1;
	}

	//------- comando !show-enemy-map -------//
	if(strcmp(comando,"!show-enemy-map") == 0){
		if(in_partita){
			stampa_mappa(sf_mappa);						//mappa avvversario con i propri attacchi
			printf("\n#");
			fflush(stdout);
		}		
		else{
			printf("%sYou are not playing a game. ",err);
			printf("\nBefore use this command you must start a game with the command !connect client_name.\n");
			printf("\n>");
			fflush(stdout);
		}
		return 1;
	}

	//------- comando !hit -------//
	if(strcmp(comando, "!hit")==0){
		//nessuna partita in corso
		if(!in_partita){
			printf("%sYou are not playing a game. ",err);
			printf("\nBefore use this command you must start a game with the command !connect client_name.\n");
			printf(">");
			fflush(stdout);
			return 1;
		}
		
		//non è il proprio turno
		if(!turno){
			printf("%s It's not your turn!",err);
			printf("\n#");
			fflush(stdout);
			return 1;
		}		
		
		//coordinate non valide
		if((parametro[0]<'A'||parametro[0]>'J') || (parametro[1]<'0'||parametro[1]>'9') || parametro[2]!='\0'){ 
			printf("%sCoordinates not valid.\n",err);
			printf("The coordinates must be in the format [A-J][0-9]\n#"); 
			fflush(stdout);
			return 1;
		}
		
		//invio comando all'avversario
		memset(buf, '\0', max_buf);
		strcpy(buf, comando);
		check= sendto(sck_UDP, buf, max_buf-1, 0, (SA*)&UDP_addr, sizeof(UDP_addr));
		if((check==-1) || (check<4)) {
			printf("%sFailed to send the command.\n",err);
			exit(1);		
		}	

		//invio coordinate all'avversario
		memset(buf, '\0', max_buf);		
		strcpy(buf, parametro);
		check= sendto(sck_UDP, buf, max_buf-1, 0, (SA*)&UDP_addr, sizeof(UDP_addr));
		if((check==-1) || (check<4)) {
			printf("%sFailed to send the coordinates.\n",err);
			exit(1);		
		}
	
		//ricezione risultato attacco
		memset(buf, '\0', max_buf);		
		check= recvfrom(sck_UDP, buf, max_buf-1, 0, (SA*)&UDP_addr, &len_UDP_addr);
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive the result of the assault.\n",err);
			exit(1);
		}
		buf[check]=' ';
	
		//conversione coordinate da caratteri a interi
		x= parametro[0]-65;								//coordinata x (colonna)
		y= parametro[1]-48;								//coordinata y (riga)

		printf("%s says: ",sfidato_nome);
		
		//controllo attacco
		if(buf[0]=='c'){								//nave colpita
			printf("HITTED\n");
			sf_mappa[y][x]=2;
			colpito--;
			if(colpito==0)
				disconnect(1);							//il client ha affondato tutte le navi
		}
		if(buf[0]=='a'){								//acqua
			printf("WATER\n");
			sf_mappa[y][x]=3;
		}
		if(buf[0]=='s')									//nave già colpita
			printf("Boat already hitted in this coordinates.\n");
		if(buf[0]=='t')									//acqua già trovata
			printf("Water already founded in this coordinates.\n");
		if(buf[0]=='e')									//acqua già trovata
			printf("error\n");

		//cambio turno
		if(turno)
			turno=0;
		else
			turno=1;

		printf("It's %s's turn\n", sfidato_nome);
		printf("\n#");
		fflush(stdout);

		return 1; 
	}

	//------- comando !disconnect -------//	
	if(strcmp(comando, "!disconnect")==0){
		//nessuna partita in corso
		if(!in_partita){
			printf("%sYou are not playing a game.",err);
			printf("\nBefore use this command you must start a game with the command !connect client_name.\n");
			printf(">");
			fflush(stdout);
			return 1;
		}

		if(!turno){
			printf("%sIt's not your turn!",err);
			printf("\n#");
			fflush(stdout);
			return 1;
		}	

		disconnect(0);									//disconnessione
		
		return 1;
	}		
	
	//------- comando !quit -------//	
	if(strcmp(comando, "!quit")==0){
		if(in_partita){
			if(!turno){
				printf("%sIt's not your turn!",err);
				printf("\n#");
				fflush(stdout);
				return 1;
			}	
			
			disconnect(4);					//se si sta giocando una partita prima si esegue una disconnessione

			//in disconnect 'comando' viene riallocato con !disconnect
			comando=realloc(NULL,strlen("!quit")+1);
			strcpy(comando, "!quit");
			comando[strlen("!quit")]= '\0'; 
		}

		//invio comandi al server (lun+comando)
		lun= strlen(comando);
		check = send(sck, (void*) &lun, 4, MSG_WAITALL);
		if((check==-1)||check<4){
			printf("%sFailed to send command length\n",err);
			exit(1);
		}
		check = send(sck, (void*)comando, lun, 0);
		if(check==-1 || check <lun){
			printf("%sFailed to send command.\n",err);
			exit(1);
		}
			
		//chiusura socket TCP con il server
		check = close(sck);
		if(check!=0){
			printf("%sFailed to close TCP socket",err);
			exit(1);			
		}

		printf("\nClient disconnected.\n");

		return 0;
	}

	//------- comando non valido -------//	
	printf("%s COMMAND NOT VALID\n\n",err);
	if(in_partita){
		printf("\n#");
		fflush(stdout);	
	}
	else{
		printf(">");
		fflush(stdout);	
	}

	return 1;	
}

//------- comando non valido -------//
void disconnect(int mot)
{
	//leggenda
	//0 richiesta di disconnessioni esplicita del client
	//1 fine partita - la partita è stata vinta 
	//2 disconnessione automatica - non è stato ricevuto nient per più di un minuto
	//3 fine partita - la partita è stata vinta dall'avversario
	//4 comando quit

	comando=realloc(NULL, strlen("!disconnect")+1);
	strcpy(comando, "!disconnect");
	comando[strlen("!disconnect")]= '\0'; 	

	//invio comando all'avversario
	memset(buf, '\0', max_buf);
	strcpy(buf, comando);
	check= sendto(sck_UDP, buf, max_buf-1, 0, (SA*)&UDP_addr, sizeof(UDP_addr));
	if((check==-1) || (check<4)) {
		printf("%sFailed to send a command.\n",err);
		exit(1);		
	}

	//chiudo il socket UDP con l'avversario
	check = close(sck_UDP);
	if(check!=0){
		printf("%sFailed to close UDP socket",err);
		exit(1);			
	}			

	FD_CLR(sck_UDP, &master);						//rimuovo socket_UDP da master
	
	in_partita=0;

	//invio comandi al server (lun+comando)
	lun= strlen(comando);
	check = send(sck, (void*) &lun, 4, MSG_WAITALL);
	if((check==-1)||check<4){
		printf("%sFailed to send command length\n",err);
		exit(1);
	}
	check = send(sck, (void*)comando, lun, 0);
	if(check==-1 || check <lun){
		printf("%sFailed to send command.\n",err);
		exit(1);
	}							

	printf("\nYou are now disconnected");

	if(mot==0)
		printf(": YOU LOSE BECAUSE YOU CLOSE THE GAME\n");
	if(mot==1)
		printf(": YOU WIN THE GAME\n");
	if(mot==3)
		printf(": YOUR ENEMY WON THE GAME\n");

	if(mot!=4){
		printf("\n>");
		fflush(stdout);
	}

	free(sfidato_nome);
	free(sfidato_indirizzo);

	return;	
}

//------- inserimento coordinate -------//
void in_coord(int var, int h)
{	
	printf("Please insert the coordinates for the boat %d of length %d: ", var, h);
	fflush(stdin);									//svuotamento buffer ingresso per eventuali caratteri spuri
	scanf("%s", coord); 							//lettura coordinata
		
	//controllo sintassi coordinate
	while( (coord[0]<'A' || coord[0]>'J') || (coord[1]<'0' || coord[1]>'9') || coord[2]!='\0' ){ 
		printf("%sCoordinates not valid\n",err);
		printf("The coordinates must be in the format [A-J][0-9]\n"); 

		//richiedo le coordinate
		printf("Please insert the coordinates for the boat %d of length %d: ", var, h);
		fflush(stdin);				
		scanf("%s", coord); 
	}
	return;
}

//------- inserimento orientamento -------//
void in_orien(int var, int h)
{	
	printf("Please insert the orientation for the boat %d of length %d: ",var, h);
	fflush(stdin);									//svuotamento buffer ingresso per eventuali caratteri spuri
	scanf("%s", orient); 							//lettura orientamento
		
	//controllo sintassi orientamento
	while(!(orient[0]=='O' || orient[0]=='V') || orient[1]!='\0'){	
		printf("%sOrientation not valid\n",err);
		printf("The orientation must be O for Horizontal or V for Vertical\n"); 

		//richiedo l'orientamento
		printf("Please insert the orientation for the boat %d of length %d: ", var, h);
		fflush(stdin);				
		scanf("%s", orient);
	}
	return;
}

//------- stampa mappa -------//
void stampa_mappa(int map[LUN][LUN])	
{	
	//leggenda
	//0= vuoto	--> .
	//1= nave --> n
	//2= colpito --> X
	//3= acqua --> O
	
	printf("\n    A  B  C  D  E  F  G  H  I  J\n");	//stampa lettere delle colonne
	printf("   -----------------------------\n");
	
	for(rig = 0; rig < LUN; rig++){
		printf("%d | ", rig);							//stampa numeri di riga
    		for(col = 0; col < LUN; col++){
				if(	map[rig][col] ==0)
					printf(".  ");
				if(	map[rig][col] ==1)
					printf("n  ");
				if(	map[rig][col] ==2)
					printf("X  ");
				if(	map[rig][col] ==3)
					printf("O  ");
			}				
		printf("\n");
	}
	return;
}

//------- costruzione mappa -------//
void costruisci_mappa(int j, int var)
{
	int i;
	check1=check2=1;
	
	//le coordinate vengono prese dall'array dopo essere state inserite dall'utente
	x= coord[0]-65;										//coordinata x (colonna)
	y= coord[1]-48;										//coordinata y (riga)

	//azzeramento mappa - inizialmente la mappa è vuota
	for(rig = 0; rig < LUN; rig++)
    		for(col = 0; col < LUN; col++)
      			orig_mappa[rig][col]==0;

	x1=x2=x;
	y_1=y2=y;

	//j - indica la grandezza della nave, quindi quante celle devo aggiornare
	//controllo se l'utente aveva già posizionato altre navi nelle celle delle nuove coordinate
	while(check1){	
		for(i=0; i<j;i++){
			if (orig_mappa[y_1][x1]!=1){					//se =1 vuol dire che c'era un'altra nave
				if(orient[0]=='O') x1++;				
				if(orient[0]=='V') y_1++;				
			}
			else
				check2=0;
		}

		if (!check2){
			printf("%sThe boat is on top of another boat, try another position.\n",err);
			in_coord(var, j);								//richiedo coordinate
			x1=x2= coord[0]-65;
			y_1=y2= coord[1]-48;
			in_orien(var, j);								//richiedo orientamento
			check2=1;
			continue;						
		}
		else 
			check1=0;
	}
	
	//controllo se l'utente ha posizionato la nave fuori dalla mappa
	check1=check2=1;
	while(check1){	
		if(orient[0]=='O') 
			if((x2+j)>10)
				check2=0;				
		if(orient[0]=='V') 
			if((y2+j)>10)
				check2=0;				
	
		if (!check2){
			printf("%sThe boat doesn't fit in the map, try another position.\n",err);
			in_coord(var,j);								//richiedo coorrdinate		
			x2= coord[0]-65;
			y2= coord[1]-48;
			in_orien(var, j);								//richiedo orientamento
			check2=1;
			continue;						
		}
		else 
			check1=0;
	}
	
	//inserimento nave nella mappa
	for(i=0; i<j;i++){			
		orig_mappa[y2][x2]=1;	
		if(orient[0]=='O') x2++;
		if(orient[0]=='V') y2++;
	}

	return;
}


