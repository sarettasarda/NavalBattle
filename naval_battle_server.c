#include<stdlib.h>									//gestore memoria dinamica
#include<stdio.h>									//gestore dei file
#include<string.h>									//gestore stringhe
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<netinet/in.h>								// serve per avere la struttura sockaddr_in e struct in_addr

#define SA struct sockaddr
#define MAX_CLIENTS 10

//------- struttura gestione Client -------//
struct Client {	
	int libero;											//posizione dell'array: libera(1) - occupata(0)
	int occupato;										//client occupato in un'altra partita(0) - libero (1) 
	int accept_socket;									//socket TCP connessione con il server
	int porta_UDP;										//porta di ascolto UDP per connesione con client
	char* indirizzo;									//indirizzo ip del client in formato dotted decimal
	char* nome;											//nome client
	int indice;											//indice nell'array 
	int indice_sfidato;									//indice client sfidato
};

//------- variabili -------//
struct Client array_Client[MAX_CLIENTS];				//struttura contenente i dati dei client

int liberi=MAX_CLIENTS;									//client liberi - numero posizioni libere nell'array
int occupati=0;											//quanti client connessi al server (MAX_CLIENTS-liberi)
int backlog=MAX_CLIENTS;						//dimensione massima coda di connessioni in attesa della accept

char* err= "[ERROR] ";									//variabile per l'invio della parola errore

int fdmax;												//massimo numero di descrittori di file per select 
int yes = 1;											//per setsockopt() SO_REUSEADDR
fd_set read_fds;								// lista dei secrittori di file temporanea per  select() (temp)

/* maste - lista dei descrittori di file principale (master), contiene tutti i descrittori di file che sono attualmente connessi, e anche il descrittore che è in attesa di nuove connessioni*/	
fd_set master;	

char* addr;											//indirizzo del client convertito in formato dotted decimal
char* porta;											//porta di connessione server passato al main
char* host;												//host di connessione del server passato al main
int s_porta;											//porta di connessione del server convertita in intero
/*endptr - nella conversione della porta, punta al primo carattere successivo alla porzione di stringa che è stata convertita*/
char* endptr; 		
		
int listen_sck;											//socket di ascolto del server
/*sk - socket usato per la connessione client-server, ogni client dopo l'accept(...) ha il suo socket di comunicazione con il server, memorizzato nella propria struttura dati*/
int sk;													
int check;												//variabile controllo errori	
int c_porta;											//porta di connessione del client convertita in intero
struct sockaddr_in srv_addr;							//struttura dati contenente l'indirizzo del server
struct sockaddr_in client_addr;							//struttura dati contenente l'indirizzo del client
	
int lun;												//variabile per invio/ricezione lughezza parametri
char* par;												//variabile per invio/ricezione parametri

int client_sfidato;										//indice client sfidato (per il comando !connect)
int i, j, k, i_sel, vero, disp, var, indice;			//variabili

//------- funzioni -------//
//chiudi - chiudere correttamente il client, passato per indice, quando si hanno degli errori o si esegue !quit
void chiudi (int indice);
//comandi - esegue i comandi inviati dal client					
void* comandi(char* comando, int indice);

//------- main -------//
int main(int n_arg, char** arg)
{
	//------- inizializzazioni -------//
	FD_ZERO(&master);					//inizializza l’insieme di descrittori di master con l’insieme vuoto
	FD_ZERO(&read_fds);					//inizializza l’insieme di descrittori di read_fds con l’insieme vuoto

	if (n_arg!=3){
		printf("%sThe right syntax to start naval_battle_server is: ",err); 			
		printf("./naval_battle_server <host> <port>\n\n");
		exit(1);
	}
	
	host= arg[1];										//memorizzazione indirizzo IP del server
	porta=arg[2];										//memorizzazione porta di ascolto del server
 	s_porta=strtol(porta,&endptr,10);		   	//converte la stringa puntata da 'porta' in un long, in base 10

	if( s_porta<1024 || s_porta>65535){					//controllo correttezza porta passata al main
			printf("%sThe remote port must be in the range [1024, 65535]\n\n",err);
			exit(1);
	}

	memset(&srv_addr, 0, sizeof(srv_addr));				//azzeramento struttura dati indirizzo del server
	srv_addr.sin_family= AF_INET;						//famiglia IPv4 address
	/*htons - l'indirizzo IP e il numero di porta devono essere nel formato network order in modo da essere 	indipendenti dal formato usato dal calcolatore*/
	srv_addr.sin_port = htons(s_porta);					//porta di ascolto del server

	/*inet_pton - converte 'host' da formato presentation (127.0.0.1) a formato numeric (00011101010101010) e lo memorizzza in 'srv_addr.sin_addr', serve a settare l'host del server*/
	check = inet_pton(AF_INET, host, &srv_addr.sin_addr);
	if(check==0){
		printf("%sThe host address must be 4 numbers in the range [0-255] separated by dots.\n", err);
		printf("es. 127.0.0.1\n\n");
		exit(1);
	} 

	for(i=0; i<MAX_CLIENTS; i++){						//inizializzazione array_Client
		array_Client[i].libero = 1;						//tutti i posti dell'array liberi
		array_Client[i].indice=i;						//indice sequenziale ai posto dell'array
		array_Client[i].occupato = 1;					//nessun client impegnato in partite
		array_Client[i].indice_sfidato=-1;				//indice client sfidato
		array_Client[i].nome=malloc(30);				
		memset(array_Client[i].nome, '\0', 30);			//azzeramento nome
		array_Client[i].indirizzo=malloc(30);				
		memset(array_Client[i].indirizzo, '\0', 30);	//azzeramento indirizzo
		array_Client[i].porta_UDP=-1;					//azzeramento porta UDP di ascolto
		array_Client[i].accept_socket=-1;				//azzeramento socket accettato
	}

	memset(&client_addr, 0, sizeof(client_addr));		//azzeramento struttura ind client
	
	//------- socket di ascolto -------//
	//creazione del socket di ascolto, socket TCP come da specifica
	listen_sck = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sck==-1){
			printf("%sFailed to create socket.\n\n",err);
			exit(1);
	}

	/*setsockopt - manipola le opzioni associate con un socket, elimina il messaggio d'errore "address already in use"; livello: SOL_SOCKET (socket), opzione da settare:SO_REUSEADDR (permette di fare una bind su una certa porta anche se esistono delle connessioni established che usano quella porta), valore dell'opzione: 1*/
	check=setsockopt(listen_sck, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	if(check==-1){ 
		printf("%sFailed setsockopt() method \n\n",err);
		exit(1);
	}

	//------- bind -------//
	//collegamento indirizzo locale al socket listen_sck
	check = bind (listen_sck, (SA*) &srv_addr, sizeof(srv_addr));
	if(check!=0) {
		printf("%s Failed to start the server, try using another port.\n\n",err);
		exit(1);
	}
	
	//------- connessione stabilita -------//
	lun=strlen("clear");	
	par=malloc(lun+1);
	strcpy(par, "clear");
	par[lun]='\0';
	system(par);										//pulizia schermata terminale	
	
	printf("Server address: %s (Port: %d) \n" , host, s_porta);
		
	//------- listen -------//
	//mette il listen_sck in attesa di eventuali connessioni
	check = listen(listen_sck, backlog);				//siamo in ascolto
	if(check!=0){
		printf("%sFailed listen() method.\n",err);
		exit(1);
	}
	
	/*FD_SET() - aggiunge listen_sck all’insieme di descrittori master mettendo a 1 il bit relativo a listen_sck*/
	FD_SET(listen_sck, &master);		

	/*fdmax - tiene traccia del valore massimo dei descrittori in uso, finora è listen_sck */		
	fdmax = listen_sck; 						

	while(1){

		//------- select -------//
		read_fds = master;								//copia master in read_fds
		check=select(fdmax+1, &read_fds, NULL, NULL, NULL);
		if(check == -1){ 
			printf("%sFailed select() method.\n",err); 
			exit(1); 
		}
		
		//cicla tra le connessioni esistenti in attesa di dati da leggere (read_fds)
		for(i_sel=0; i_sel<=fdmax; i_sel++){
			/*FD_ISSET - al ritorno di select(), controlla se i_sel appartiene all’insieme di descrittori read_fds, verificando se il bit relativo a i_sel è pari a 1*/
			if(FD_ISSET(i_sel, &read_fds)){ 
				if(i_sel == listen_sck){
					//------- nuova connessione - accept -------//
					//client_addr - puntatore alla struttura che sarà riempito con l’indirizzo del client 
					lun = sizeof(client_addr); 			//client_addr va passato per riferimento
					sk = accept(listen_sck, (SA *)&client_addr, &lun); 
					if(sk== -1){ 
						printf("%sFailed to create the communication socket.\n",err);
						exit(1);  
					}
					
					FD_SET(sk, &master);				//aggiunge sk al master set	
					if(sk > fdmax)						//tiene traccia del più grande descrittore
						fdmax = sk;

					//inet_ntoa - converte l'indirizzo IP del client in una stringa dotted decimal (x1.x2.x3.x4)
					addr=inet_ntoa((struct in_addr) client_addr.sin_addr); 
					printf("Client %s connected, port %d\n",addr ,s_porta);
					
					//------- gestione nuovo client -------//
					//verifico lo stato dell'array, mi accerto che ci siano posti disponibili per il nuovo client
					if(liberi==0){						//array pieno, rifiuto la connessione					
						check=send(sk , (void*) &liberi, 4, MSG_WAITALL);	
						if(check==-1 || check<4){
							printf("%sFailed to send the message 'no more space available'.\n\n",err);
							close(sk);					//chiudo il socket TCP	
							exit(1);
						}
						printf("%sMaximum client connection capacity reached on this server\n",err);
					}
					else {								//posto disponibile, accetto la connessione
						check=send(sk ,(void*) &liberi, 4, MSG_WAITALL);
						if(check==-1 || check<4){
							printf("%sFailed to send the message about the number of positions available.\n\n",err);
							close(sk);	
							exit(1);
						}
						
						liberi--;						//aggiornamento numero posti array dispponibili
						occupati++;						//aggiornamento numero client connessi

						//------- assegnazione struttura -------//
						disp=0; 						//disp = primo elemento dell'array vuoto
						for(k=0; k<MAX_CLIENTS; k++)	//cerco elemento dell'array disponibile
							if(!array_Client[k].libero)
								disp++;
								
						array_Client[disp].indirizzo = realloc(NULL,strlen(addr)+1);//memorizzazione indirizzo
						strcpy(array_Client[disp].indirizzo, addr);
						array_Client[disp].indirizzo[strlen(addr)]='\0';
						array_Client[disp].accept_socket=sk ;			//memorizzazione socket di connessione 
						array_Client[disp].libero=0;	//posizione elemento array occupata
						
						//invio al client il suo indirizzo IP, prima la lunghezza poi l'IP come da specifica
						lun= strlen(addr);
						check=send(sk, (void*)&lun,4,MSG_WAITALL);	
						if((check==-1) || (check<4)) {
							printf("%sFailed to send the IP address length.\n",err);
							chiudi(disp);
							exit(1);	
						}	
						check=send(sk,(void*)addr,lun, 0);		
						if(check==-1 || check <lun){
							printf("%sFailed to send the IP address.\n",err);
							chiudi(disp);
							exit(1);	
						}

						//------- ricezione nome client -------//
						//(lavoro con le variabili disp=indice, sk=socket e addr=indirizzo)
						do{
							var=1;						//var tiene traccia della ricezione corretta del nome
							check=recv(sk, (void*)&lun,4,MSG_WAITALL);		
							if((check==-1) || (check<4)) {
								printf("%sFailed to receive name length.\n",err);
								chiudi(disp);					
								exit(1);
							}
							par = realloc(NULL,lun+1);		//occupazione memoria dinamica per il nome
							check=recv(sk, (void*)par, lun, 0);					
							if((check==-1) || (check<4)) {
								printf("%sFailed to receive name.\n",err);
								chiudi(disp);
								exit(1);
							}
							par[lun] = '\0';
							
							//controllo se il nome era stato già scelto da un'altro client
							j=0;
							for(k=0; k<occupati; k++){	
								if(array_Client[j].libero)	//se client libero lo salto
									j++;
					
								if(strcmp(array_Client[j].nome, par) == 0){		//nome ricevuto già esistente
									printf("%sName already in use.\n",err);
									var=0;
									break;
								}
								j++;
							}
							
							//comunico al client se deve digitare un'altro nome o meno
							check=send(sk , (void*) &var, 4, MSG_WAITALL);	
							if(check==-1 || check<4){
								printf("%sFailed to send the name availability.\n\n",err);
								chiudi(disp);
								exit(1);
							}
							
						}while(!var);

						//nome ricevuto correttamente, il nome è in par
						array_Client[disp].nome = realloc(NULL,strlen(par)+1); //memorizzazione nome
						strcpy(array_Client[disp].nome, par);
						array_Client[disp].nome[strlen(par)]='\0';

						printf("%s is connected\n", array_Client[disp].nome);
						printf("%s is free\n", array_Client[disp].nome);

						//------- ricezione porta UDP -------//
						check=recv(sk, (void*)&lun,4,MSG_WAITALL); 
						if((check==-1) || (check<4)) {
							printf("%sFailed to receive UDP port length.\n",err);
							chiudi(disp);
							exit(1);
						}
						par= realloc(NULL,lun+1);				//occupazione memoria dinamica per la porta
						check = recv (sk, (void*)par, lun, 0);					
						if((check==-1) || (check<4)) {
							printf("%sFailed to receive UDP port.\n",err);
							chiudi(disp);
							exit(1);
						}
						par[lun] = '\0';
	   
						//converte la stringa puntata da 'par' in un long, in base 10
						c_porta=strtol(par, &endptr, 10); 
						array_Client[disp].porta_UDP = c_porta;
					}					
				}			
				else{
					//------- ricezione comandi dai client -------//
					//con i_sel identifico il client che ha inviato il comando
					for(j=0; j<=MAX_CLIENTS; j++)
						if(array_Client[j].accept_socket == i_sel){
							indice=j;
							break;
						}

					check=recv(i_sel, (void*)&lun,4,MSG_WAITALL);		//ricezione lunghezza comando
					if((check==-1) || (check<4)) {
						printf("%sFailed to receive command length.\n",err);
						chiudi(indice);
						exit(1);
					}					
					par= realloc(NULL,lun+1);					//occupazione memoria dinamica per il comando
					check=recv(i_sel, (void*)par, lun, 0);				//ricezione comando
					if((check==-1) || (check<4)) {
						printf("%sFailed to receive command.\n",err);
						chiudi(indice);
						exit(1);
					} 		
					par[lun] = '\0';			

					//------- comando -------//
					comandi(par, indice);						
				}
			}
		}
	}
	for(i=0; i<MAX_CLIENTS; i++){	
		free(array_Client[i].nome);				
		free(array_Client[i].indirizzo);
	}

	close(listen_sck);									//chiudo il socket di ascolto
			
	return 0;
}

//------- chiusura client -------//
void chiudi (int indice)
{
	FD_CLR(array_Client[indice].accept_socket, &master);//rimozione socket dal master set
	
	j=0;
	for(i=0; i<=fdmax; i++)								//aggiornamento fdmax
		if(FD_ISSET(i, &master))
			j=i;

	fdmax=j;
	
	close(array_Client[indice].accept_socket);			//chiusura socket TCP con il client	
	liberi++;											//incremento posti disponibili nell'array
	occupati--;											//aggiornamento client connessi al server

	printf("%s is disconnected.\n", array_Client[indice].nome);
		
	array_Client[indice].libero=1;						//libero la posizione dell'array
	array_Client[indice].indice_sfidato=-1;				//aggiornamento client sfidato
	array_Client[indice].nome=realloc(NULL,30);				
	memset(array_Client[indice].nome, '\0', 30);		//azzeramento nome
	array_Client[indice].indirizzo=realloc(NULL,30);				
	memset(array_Client[indice].indirizzo, '\0', 30);	//azzeramento indirizzo
	array_Client[indice].porta_UDP=-1;					//aggiornamento client sfidato
	array_Client[indice].accept_socket=-1;				//aggiornamento client sfidato
		
	return;
}

//------- comandi -------//
void* comandi(char* comando, int indice)
{
	//comando - stringa che identifica il comando da eseguire
	//indice - indice nell'array del client che ha richiesto il comando

	//------- comando !who -------//
	if(strcmp(comando,"!who") == 0){
		printf("%s sent the command: !who\n", array_Client[indice].nome);
				
		//invio al client il numero di client connessi al server
		check = send(array_Client[indice].accept_socket, (void*) &occupati, 4, MSG_WAITALL);
		if((check==-1)||check<4){
				printf("%sFailed to send the number of client connected to the server.\n",err);
				chiudi(indice);
				exit(1);	
		}
		
		//invio al client i nomi dei client connessi
		j=0;
		for(i=0; i<occupati; i++){	
			if(array_Client[j].libero)					//salto posizioni dell'array vuote
				j++;
	
			//invio al client il nome del client connesso (lun + nome)
			lun= strlen(array_Client[j].nome);
			check=send(array_Client[indice].accept_socket, (void*)&lun,4,MSG_WAITALL);	
			if((check==-1) || (check<4)) {
				printf("%sFailed to send client name length.\n",err);
				chiudi(indice);		
				exit(1);
			}	
			check=send(array_Client[indice].accept_socket,(void*)array_Client[j].nome,lun, 0 );		
			if(check==-1 || check <lun){
				printf("%sFailed to send client name.\n",err);
				chiudi(indice);	
				exit(1);
			}

			j++;										//invio il nome successivo
		}
	}

	//------- comando !connect nome_client -------//
	if(strcmp(comando,"!connect") == 0){
		printf("%s sent the command: !connect\n", array_Client[indice].nome);
				
		//ricezione nome client da sfidare (lun + nome)
		check=recv (array_Client[indice].accept_socket, (void*) &lun, 4, MSG_WAITALL);
		if((check==-1) || (check<4)) {
			printf("%sFailed to receive enemy name length.\n",err);
			chiudi(indice);
			exit(1);	
		}
		par=realloc(NULL,lun+1);
		check=recv(array_Client[indice].accept_socket, (void*) par, lun, 0 );		
		if((check==-1) || (check<lun)) {
			printf("%sFailed to receive enemy name.\n",err);
			chiudi(indice);	
			exit(1);
		}
		par[lun] = '\0';
		//par - contiene il client con cui ci si vuole connettere
		
		//controllo se il parametro passato è il nome di un client connesso
		j=vero=0;
		for(i=0; i<occupati; i++){	
			if(array_Client[j].libero)					//salto posizioni dell'array vuote
				j++;
				
			if(strcmp(array_Client[j].nome, par) == 0){	//nome client che si vuole sfidare esiste
				client_sfidato=array_Client[j].indice;	//memorizzo indice dell'array del client sfidato 
				vero=1;	
				break;
			}

			j++;
		}
		
		//se non esiste nessun client con il nome client sfidato scrivo errore in par
		if(!vero){
			printf("%sThere is not a client named %s\n",err, par);
			lun=strlen("error");	
			par=realloc(NULL,lun+1);
			strcpy(par, "error");
			par[lun]='\0';
		}
		//par - contiene la stringa "errore" (se il client non esiste) o il nome del client sfidato
				
		//invio nome del client_sfidato o errore al client che ha fatto richiesta (lun+nome) 
		lun= strlen(par);
		check=send(array_Client[indice].accept_socket, (void*)&lun,4,MSG_WAITALL);	
		if((check==-1) || (check<4)) {
			printf("%sFailed to send client name length or error label length\n",err);
			chiudi(indice);	
			exit(1);	
		}	
		check=send(array_Client[indice].accept_socket,(void*)par,lun, 0 );		
		if(check==-1 || check <lun){
			printf("%sFailed to send client name or error label\n",err);
			chiudi(indice);	
			exit(1);
		}
		
		if(!vero)
			return;					//non esiste nessun client con il nome del client sfidato - esco da !connect
			
		//client sfidato esistente - controllo se occupato in un'altra partita
		//lun=0 occupato  - lun=1 libero
		if(!array_Client[client_sfidato].occupato){
			printf("%s is busy with another game.\n", par);
			
			
			lun= 0;										//invio 0 per dire che il client sfidato è occupato
			check=send(array_Client[indice].accept_socket, (void*)&lun,4,MSG_WAITALL);	
			if((check==-1) || (check<4)) {
				printf("%sFailed to send client busy.\n",err);
				chiudi(indice);	
				exit(1);	
			}
			return;										//il client sfidato è già occupato - esco da !connect
		}
		else{
			//client sfidato esistente e libero
			lun= 1;										//invio 1 per dire che il client sfidato è libero
			check=send(array_Client[indice].accept_socket, (void*)&lun,4,MSG_WAITALL);	
			if((check==-1) || (check<4)) {
				printf("%sFailed to send client busy\n",err);
				chiudi(indice);	
				exit(1);	
			}

			//invio al client sfidato il nome del client che ha chiesto la sfida (lun+nome)
			lun= strlen(array_Client[indice].nome);
			check=send(array_Client[client_sfidato].accept_socket, (void*)&lun,4,MSG_WAITALL);	
			if((check==-1) || (check<4)) {
				printf("%sFailed to send challenger name length.\n",err);
				chiudi(indice);	
				exit(1);	
			}
			check=send(array_Client[client_sfidato].accept_socket,(void*)array_Client[indice].nome,lun, 0);		
			if(check==-1 || check <lun){
				printf("%sFailed to send challenger name length.\n",err);
				chiudi(indice);	
				exit(1);
			}

			//risposta del client sfidato
			check=recv (array_Client[client_sfidato].accept_socket, (void*)&lun, 4, MSG_WAITALL);
			if((check==-1) || (check<4)) {
				printf("%sFailed to receive the answer.\n",err);
				chiudi(indice);	
				exit(1);
			}

			//se lun=0 partita rifiutata - lun=1 partita accettata
			//invio a chi ha fatto la !connect la risposta del client sfidato
			check=send(array_Client[indice].accept_socket, (void*)&lun ,4, MSG_WAITALL);	
			if((check==-1) || (check<4)) {
				printf("%sFailed to send the answer.\n",err);
				chiudi(indice);	
				exit(1);	
			}

			if(!lun){										//sfida non accetttata
				printf("%s declined the %s challange \n",array_Client[client_sfidato].nome ,array_Client[indice].nome);
				
				return;
			}
			else{											//sfida accettata				

				printf("%s is connected with %s\n", array_Client[indice].nome,array_Client[client_sfidato].nome );
				
				//invio porta UDP di ascolto del client sfidato
				lun=array_Client[client_sfidato].porta_UDP;
				check=send(array_Client[indice].accept_socket, (void*)&lun ,4, MSG_WAITALL);	
				if((check==-1) || (check<4)) {
					printf("%sFailed to send UDP port.\n",err);
					chiudi(indice);		
					exit(1);
				}
	
				//invio indirizzo IP del client sfidato (lun + IP)
				lun= strlen(array_Client[client_sfidato].indirizzo);
				check=send(array_Client[indice].accept_socket, (void*)&lun,4,MSG_WAITALL);	
				if((check==-1) || (check<4)) {
					printf("%sFailed to send IP address length.\n",err);
					chiudi(indice);
					exit(1);		
				}	
				check=send(array_Client[indice].accept_socket,(void*)array_Client[client_sfidato].indirizzo,lun, 0 );		
				if(check==-1 || check <lun){
					printf("%sFailed to send IP address.\n",err);
					chiudi(indice);	
					exit(1);
				}

				//memorizzo indici client sfidati 
				array_Client[client_sfidato].indice_sfidato=array_Client[indice].indice;
				array_Client[indice].indice_sfidato=array_Client[client_sfidato].indice;
				
				//------- inizio partita -------//
			}
		}
		return;
	}
		
	//------- comando !diconnect -------//
	if(strcmp(comando,"!disconnect") == 0){
		printf("%s is now disconnected from %s\n", array_Client[indice].nome, array_Client[array_Client[indice].indice_sfidato].nome);

		array_Client[indice].occupato=1;
		printf("%s is free\n",array_Client[indice].nome);

		array_Client[array_Client[indice].indice_sfidato].occupato=1;
		printf("%s is free\n",array_Client[array_Client[indice].indice_sfidato].nome);
		
		return;
	}

	//------- comando !quit -------//
	if(strcmp(comando,"!quit") == 0){
		chiudi(indice);
		return;
	}
}

