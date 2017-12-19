/************************************************************************************************
 ******************************************** Projecto RCI **************************************
 ******************************************** 2ºS 2016/17 +**************************************
 *  Servidor de mensagens                                                                       *
 *  Principais funções:                                                                         *
 *   - Manter contacto com o servidor de indentidades                                           *
 *   - Receber mensagens via UDP de um cliente usando um protocolo pré definido                 *
 *   - Guardar as mensagens submetidas pelos clientes                                           *
 *   - Sincronização automatica entre servidores usando TCP                                     *
 *   - Auto replicação das mensagens para os outros servidores                                  *
 ************************************************************************************************
 ************************************************************************************************
 ********************************* By: David Teles e Lorenço Pato *******************************
 ************************************************************************************************
 ************************************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>


#define DEFAULT_IP "tejo.tecnico.ulisboa.pt"
#define DEFAULT_PORT 59000
#define DEFAULT_STORAGE 200
#define DEFAULT_INTERVAL 10
#define MIN_REQUIRED 4
#define MAX_MESSAGE_SIZE 140
#define MAX_NUM_SERVERS 32
#define TIMEOUT_ 2

#define max(A,B) ((A)>=(B)?(A):(B))

typedef struct SERVER_ {
	char name[20];
    char serverip[100];
	struct in_addr ip;
	int upt, tpt;
	struct hostent *host;
} SERVER;

typedef struct MESSAGE_ {
	char msg[MAX_MESSAGE_SIZE];
	int time;
} MESSAGE;

typedef struct SOCKET_
{
	int fd;
	struct sockaddr_in addr;
} SOCKET;


int help();
void chopN(char *str, size_t n);
int join (int, SERVER, struct sockaddr_in);
int getservers(int fd, struct sockaddr_in IDserveraddr);
void receiveMessage(int, MESSAGE*);
void sendLatestMessages(int, MESSAGE *messagelist, int , struct sockaddr_in);
void userInput(int, SERVER, struct sockaddr_in, MESSAGE*);
void storeMessage(char *msg, MESSAGE *messagelist, int print);
void printServers();
void printMessages();
void InitializeSocketList();
void connectToServers();
void sendTCP(int fd, char* str);
void printConnectedServers();
int relaymsg(char *msg, int clock);
int receiveTCPmsg(MESSAGE *msglist, SOCKET *socketlist, int);
int SGetMessages(MESSAGE *messagelist);

// list for all known servers - connected and not connected(fd=-1)
SOCKET socketlist[MAX_NUM_SERVERS];
int socketlistsize=0;

// registered servers in ID server
SERVER serverlist[MAX_NUM_SERVERS];
int sl_size=0;

int maxml_size = DEFAULT_STORAGE; // max storage for message list
int ml_size=0; // message list size
int LC = 0;	// Logic Clock
int fd_stcp=-1;	// server TCP file descriptor
char NAME[20]="\0";

int main(int argc, char *argv[]) {

	int i, j, fd, ret;
	SERVER msgserv, sid;
	struct in_addr *a;
	struct sockaddr_in IDserveraddr, myaddr, TCPserveraddr, TCPclientaddr;
	fd_set rfds;
	int maxfd, counter, newfd;
	unsigned int TCPclientlen;
	struct timeval timeout;

	/* Initialize default values for m and r */
	int m = DEFAULT_STORAGE;
	int r = DEFAULT_INTERVAL;

	/* Initialize ID server with default values */  
	sid.host = gethostbyname(DEFAULT_IP);		      
	a=(struct in_addr*)sid.host->h_addr_list[0];
    sid.ip.s_addr = a->s_addr;	
	sid.upt = DEFAULT_PORT;
	
	if (argc < MIN_REQUIRED) return help();
	
	// Initialize socket list with -1
	InitializeSocketList();
   
   /* read arguments */
   for (i = 1; i < (argc - 1); i++) {
   		//	name
       if (strcmp("-n", argv[i]) == 0) {
		  strcpy(msgserv.name, argv[++i]);
		  strcpy(NAME, msgserv.name);
          continue;
       }
       //	IP
       if (strcmp("-j", argv[i]) == 0) {
          inet_aton(argv[++i],&(msgserv.ip));
          continue;
       }
       //	UDP port
       if (strcmp("-u", argv[i]) == 0) {
          msgserv.upt = atoi(argv[++i]);
          continue;
       }
       //	TCP port
       if (strcmp("-t", argv[i]) == 0) {
          msgserv.tpt = atoi(argv[++i]);
          continue;
       }
       //	ID server ip
       if (strcmp("-i", argv[i]) == 0) {
		  /* Gets host by name and stores in sid */ 
		  sid.host = gethostbyname(argv[++i]);
          if (sid.host == NULL) {
			  printf("Couldn't find ID server\n");
			  exit(0);		  
		  }
		  /* Copies sid's IP address to sid.ip */      
          a=(struct in_addr*)sid.host->h_addr_list[0];
          sid.ip.s_addr = a->s_addr;
          continue;
       }
       //	TCP port
       if (strcmp("-p", argv[i]) == 0) {
          sid.upt = atoi(argv[++i]);
          continue;
       }
       //	message list size
       if (strcmp("-m", argv[i]) == 0) {
          m = atoi(argv[++i]);
          continue;
       }
       //	refresh rate
       if (strcmp("-r", argv[i]) == 0) {
          r = atoi(argv[++i]);
          continue;
       }
       return help();
   }
	
	// Allocate message list with size m
	MESSAGE* messagelist = (MESSAGE*) malloc(m*sizeof(MESSAGE));
	maxml_size = m;
    

    // Message Time initialization
    for (i=0; i<maxml_size; i++) {
        messagelist[i].time=-1;
    }
    
	/* Initialize timeout to the refresh rate */ 
	timeout.tv_sec=r;
	timeout.tv_usec=0;

	/* open UDP socket for communication with ID server and clients */
    fd = socket(AF_INET,SOCK_DGRAM,0);
    if (fd == -1){
        printf("socket error!\n"); close(fd); exit(1);    
    }    

    /* define ID server addr */
	memset((void*)&IDserveraddr, (int)'\0', sizeof(IDserveraddr));
	IDserveraddr.sin_family=AF_INET;
	IDserveraddr.sin_addr.s_addr=((struct in_addr *)sid.host->h_addr_list[0])->s_addr;
	IDserveraddr.sin_port=htons((u_short)sid.upt);

	/* myaddr - this server's address */
	memset((void*)&myaddr, (int)'\0', sizeof(myaddr));
	myaddr.sin_family=AF_INET;
	myaddr.sin_addr.s_addr=htonl(INADDR_ANY);
	myaddr.sin_port=htons((u_short)msgserv.upt);

    /* UDP bind */ 
	ret = bind(fd,(struct sockaddr*)&myaddr, sizeof(myaddr));
	if (ret == -1){
		printf("UDP Bind error!\n"); close(fd);	exit(1);
	}

	/* open TCP server socket for communication with other servers */
	fd_stcp = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_stcp == -1){
        printf("TCP socket error!\n"); close(fd_stcp); exit(1);    
    }    

	// Set to ignore SIGPIPE signal
	void(*old_handler)(int);
	if((old_handler=signal(SIGPIPE, SIG_IGN))==SIG_ERR) exit(1);

	// Alocate TCPserveraddr
    memset((void*)&TCPserveraddr, (int)'\0', sizeof(TCPserveraddr));
    TCPserveraddr.sin_family=AF_INET;
    TCPserveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
    TCPserveraddr.sin_port=htons((u_short)msgserv.tpt);
	
	// bind TCP
    ret = bind(fd_stcp,(struct sockaddr*)&TCPserveraddr, sizeof(TCPserveraddr));
    if (ret == -1){
		printf("TCP Bind error!\n"); close(fd_stcp);	exit(1);
	}

    // fd_stcp is listening for incoming handshakes
    if(listen(fd_stcp,5)==-1) {
		printf("listen error\n");	
		exit(1);
	}


	/* INITIAL COMMANDS-> get servers -> connect to servers -> s get messages
		- get servers from SID and print them to stdout
		- send join message to SID
		- connect to registered servers
		- get current messages from a registered server	*/
	getservers(fd, IDserveraddr);
	printServers();
    join(fd, msgserv, IDserveraddr);
	connectToServers();
    SGetMessages(messagelist);
	
    // enter program loop
	while(1){
		// reset rfds
		FD_ZERO(&rfds);

		// set file descriptor for UDP
		FD_SET(fd,&rfds); maxfd=fd;

		// set file descriptor for stdin (0)
	 	FD_SET(fileno(stdin),&rfds); maxfd=max(maxfd,fileno(stdin));

		// set file descriptor for TCP server
		FD_SET(fd_stcp,&rfds); maxfd=max(maxfd, fd_stcp);

		// set file descriptors for all connected servers - TCP client
		for(j=0; j < socketlistsize; j++) {
			if(socketlist[j].fd != -1) {
				FD_SET(socketlist[j].fd, &rfds);
				maxfd = max(maxfd, socketlist[j].fd);
			}
		}
		
		// enter select function
		counter=select(maxfd+1,&rfds,NULL,NULL,&timeout);
		if(counter==-1) { // error
			printf("select error\n"); 
			close(fd); exit(0);
		}
		
		// if timeout - send join message, and refresh server list
		if(counter==0){ 
			join(fd, msgserv, IDserveraddr);
			getservers(fd, IDserveraddr);
			printServers();
			printConnectedServers();
						
			// reset timeout 
			timeout.tv_sec=r;
			timeout.tv_usec=0;
		}

		// an input was given from stdin
		if(FD_ISSET(0,&rfds)) {
	        userInput(fd, msgserv, IDserveraddr, messagelist);
		}

		// Accept new message server - add file descriptor to socketlist
		if(FD_ISSET(fd_stcp, &rfds)) {

			TCPclientlen = sizeof(TCPclientaddr);
			newfd = accept(fd_stcp, (struct sockaddr*)&TCPclientaddr, &TCPclientlen);
			socketlist[socketlistsize].fd = newfd;
			socketlist[socketlistsize].addr = TCPclientaddr;

			printf("> Connected to %s\n", inet_ntoa(socketlist[socketlistsize].addr.sin_addr));

			socketlistsize++;
		}
		
		// If received TCP message
		for (j=0; j < socketlistsize; j++) {
			if(socketlist[j].fd != -1) {
				if(FD_ISSET(socketlist[j].fd, &rfds)) {
					FD_CLR(socketlist[j].fd, &rfds);
                    receiveTCPmsg(messagelist, socketlist, j);
				}
			}
		}

		// received UDP datagram from UDP socket(fd)
		if(FD_ISSET(fd,&rfds)){
			receiveMessage(fd, messagelist);
		}
	}

   return 0;
}

/** 
	Receives DATAGRAM from client via UDP
	Handles the received message
	*/
void receiveMessage(int fd, MESSAGE *messagelist){
	char buffer[MAX_MESSAGE_SIZE], msg[MAX_MESSAGE_SIZE], command[20];
	int n=0;
	struct sockaddr_in clientaddr;
	unsigned int addrlen = sizeof(clientaddr);
	
	// receive datagram from given file descriptor
	int nread=recvfrom(fd, buffer, sizeof(buffer),0,(struct sockaddr*)&clientaddr,&addrlen);
	if(nread == -1){
		printf("recvfrom error!\n"); close(fd); exit(1);
	}
	
	// if request for messages, read number of messages and send to client
	if (strstr(buffer, "GET_MESSAGES") != NULL) {
		sscanf(buffer,"%s %d", command, &n);
		printf("> Client request for %d last messages\n", n);
		// send the n latest messages to client
		sendLatestMessages(fd, messagelist, n, clientaddr);
	}
	
	// if received a publish request
	if(strstr(buffer, "PUBLISH") != NULL){
		strcpy(msg, buffer);
		// remove the string "PUBLISH " from msg, keeping only the message
		chopN(msg, sizeof("PUBLISH"));

		// store message and relay to other servers
		storeMessage(msg, messagelist,1);
        relaymsg(msg, LC);
	} 
}

/**
	reads input from user from stdin
	*/
void userInput(int fd, SERVER msgserv, struct sockaddr_in IDserveraddr, MESSAGE* messagelist) {
	char buffer[128], command[128];
	int i;
	
	// get command
	fgets(buffer,256,stdin);
    sscanf(buffer,"%s\n", command);

    // terminate program
   	if(strcmp(command,"exit")==0){
   		// close all sockets
   		for (i=0; i<socketlistsize; i++){
   			if(socketlist[i].fd != -1) close(socketlist[i].fd);
   		}
		close(fd);
		close(fd_stcp);
		free(messagelist);
       	exit(0);
            
    } else if (strcmp(command,"join")==0){
    	//Registo do servidor de mensagens no servidor de identidades
	    join(fd, msgserv, IDserveraddr);
	            
	} else if (strcmp(command,"show_servers")==0){
	    //Fazer request do numero de servers
	    getservers(fd, IDserveraddr);
	    printServers();
    } else if (strcmp(command,"show_messages")==0){
        //Listagem de todas as mensagens guardadas no servidor, ordenadas pelos tempos lógicos
        printMessages(messagelist);
    } else {
        printf("Command Not Found!\n");
    }		
}

/**
	requests ID server for other msg servers and stores them in serverlist
		fd - UDP socket
		IDserveraddr - sid address
		serverlist - vector used to store the received servers
	*/
int getservers(int fd, struct sockaddr_in IDserveraddr){
    
    int st, rf, i=0;
    unsigned int addrlen;
    char buffer[5000], ip[100], *token;
    char msg[20]="GET_SERVERS";

    // reset the size of server list
    sl_size=0;

    // Send request to ID server
    addrlen = sizeof(IDserveraddr);
    st = sendto(fd, msg, strlen(msg)+1, 0,(struct sockaddr*)&IDserveraddr, addrlen);
    if (st == -1){
        printf("SendTo error!\n");
        exit(1);
    }
    
    // Receive ID server response to buffer
    addrlen = sizeof(IDserveraddr); 
    rf = recvfrom(fd, buffer, sizeof(buffer),0,(struct sockaddr*)&IDserveraddr,&addrlen);
    if (rf == -1){
        printf("Recvfrom error!\n");
        exit(1);
    }
    
    // Process buffer and register servers ip
    buffer[rf-1]='\0';
    token=strtok(buffer, "\n");
    token=strtok(NULL, ";");
    while(token!=NULL){
        strcpy(serverlist[i].name,token);
        token=strtok(NULL, ";");
        strcpy(ip,token);
        strcpy(serverlist[i].serverip, ip);
        inet_aton(ip, &(serverlist[i].ip));
        token=strtok(NULL, ";");
        sscanf(token, "%d",&(serverlist[i].upt));
        token=strtok(NULL, "\n");
        sscanf(token, "%d",&(serverlist[i].tpt));
        token=strtok(NULL, ";");
        i++;
        sl_size++;        
    }    
    return(0);
}

/**
	joins server id as a message server
		fd - UDP socket
		msgserv -  message server info
		IDserveraddr - sid address

	message format - REG name;ip;upt;tpt 
	*/
int join (int fd, SERVER msgserv, struct sockaddr_in IDserveraddr){
	char msg[100];
	int st, addrlen;
	
	sprintf(msg, "REG %s;%s;%d;%d", msgserv.name, inet_ntoa(msgserv.ip), msgserv.upt, msgserv.tpt);
	
	addrlen = sizeof(IDserveraddr);
	st = sendto(fd, msg, strlen(msg)+1, 0, (struct sockaddr*)&IDserveraddr, addrlen);
	if (st == -1){
        printf("SendTo error!\n");
        exit(1);
    }

	printf("> Join\n");
	return 0;
}

/**
	MESSAGES\n(message\n)*
	Mensagem enviada dum servidor de mensagens para um terminal com uma lista
	de mensagens, ordenadas pelos tempos lógicos.
		- fd - socket UDP
		- messagelist - lista de mensagens
		- n - numero de mensagens pedidas
		- clientaddr - endereço do cliente
	*/
void sendLatestMessages(int fd, MESSAGE *messagelist, int n, struct sockaddr_in clientaddr) {

	int st, i;
	unsigned int addrlen;
	char msg[5000] = "MESSAGES\n", str[5000];

	// verificar se o numero de mensagens pedidas é maior que o numero de msgs armazenadas
	i = ml_size - n; 
	// in case that client asks for too many messages
	if(i < 0) i = 0;
	if(i < ml_size-n) i = 0;

	// concatenate latest messages into msg, separated by "\n"
	for(; i<ml_size; i++) {
		strcpy(str, messagelist[i].msg);
		strcat(str, "\n");
		strcat(msg, str);
	}

	// send messages to client
    addrlen = sizeof(clientaddr); 
	st = sendto(fd, msg, strlen(msg)+1, 0,(struct sockaddr*)&clientaddr, addrlen);
    if (st == -1){
        printf("SendTo error!\n");
        exit(1);
    }
}

/**
	Stores msg in the messagelist
	and increments size of messagelist, ml_size. Then prints the full message list if print=1.
	*/
void storeMessage(char *msg, MESSAGE *messagelist, int print) {
	strcpy(messagelist[ml_size].msg, msg);
	LC++;
	messagelist[ml_size].time=LC;
	ml_size++;

	// Case the storage exceeds limit, print message and clear message storage
	if(ml_size >= maxml_size) { 
		printf("Message storage is full!\n"); 
		ml_size=0;
	}
	if(print)printMessages(messagelist);
}

/**
	Initializes all socket list file descriptors to -1
	*/
void InitializeSocketList(){
	int i;
	for (i=0; i < socketlistsize; i++)
	{
		socketlist[i].fd=-1;
	}
}

/**
	Tenta conectar-se aos servidores registados no server.
	Percorre a lista de servers e tenta connectar-se com cada um deles
	Aos servidores que não conseguir ligar faz fd=-1
	Quando o servidor não tem resposta o programa fica bloqueado durante muito tempo
	A solução seria tornar o connect não bloqueante (através do fctnl e getsockpt) - Não implementado
*/
void connectToServers() {
	int i, con, j=0;
	struct sockaddr_in addr;

	printf("> Connecting to online servers\n");

	for(i=0; i<sl_size; i++) {

		// don't try to connect to itself or other "bad" servers 
		if (strcmp(serverlist[i].name,NAME) == 0 ||
			strcmp(serverlist[i].serverip,"localhost") == 0 ||
			strcmp(serverlist[i].serverip,"10.0.2.15") == 0) {

			printf("> Won't connect to %s.\n", serverlist[i].name);
			socketlist[i].fd = -1;
			socketlistsize++;
			continue;
		}

		socketlist[i].fd = socket(AF_INET, SOCK_STREAM, 0);

		// Allocate addr
		memset((void*)&addr,(int)'\0',sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr = serverlist[i].ip;
		addr.sin_port = htons((u_short)serverlist[i].tpt);
		socketlist[i].addr = addr;

		// *Set the socket non-blocking
    	// *fcntl(socketlist[i].fd, F_SETFL, O_NONBLOCK);

		con = connect(socketlist[i].fd, (struct sockaddr*)&addr, sizeof(addr));

		// If connection is made
		if (con == 0){
			socketlist[i].addr=addr;
			printf("> Connected to %s\n", serverlist[i].name);
			j++;
		}
		// If connection was unsuccessful
		else if (con == -1){
			printf("> Unable to connect to %s.\n", serverlist[i].name);
			close(socketlist[i].fd);
			socketlist[i].fd = -1;
		}

		socketlistsize++;
	}

	printf("> Connected to %d servers out of %d\n", j, sl_size);	
}

/**
	Program argument usage
	*/
int help() {
   printf("Usage: msgserv –n name –j ip -u upt –t tpt [-i siip] [-p sipt] [–m m] [–r r]\n");
   printf("\t-n: message server name\n");
   printf("\t-j: message server IP\n");
   printf("\t-u: UDP port\n");
   printf("\t-t: TCP port\n");
   printf("\t-i: IP address of ID server [Default: tejo.tecnico.ulisboa.pt]\n");
   printf("\t-p: UDP port of IP server [Default: 59000]\n");
   printf("\t-m: maximum number of stored messages [Default: 200]\n");
   printf("\t-r: refresh interval in seconds [Default: 10]\n");
   return 1;
}

/**
	Prints all messages stored in messagelist
		# message
	*/
void printMessages(MESSAGE* messagelist){
	printf("------------------------------------\n");
	printf("\t> MESSAGES:\n");
	int i;
	for (i = 0; i < ml_size; ++i)
	{
		printf("\t%d - %s\n",messagelist[i].time, messagelist[i].msg);
	}
	printf("------------------------------------\n");
}

/** 
	Prints all stored servers in the following format: 
		#	name;ip_address;UPT port;TCP port
	*/
void printServers(){
	printf("ONLINE SERVERS:\n");
	int i;
	for(i=0; i < sl_size; i++){
		printf("\t%d\t%s;%s;%d;%d\n", i+1, serverlist[i].name, inet_ntoa(serverlist[i].ip), 
									serverlist[i].upt, serverlist[i].tpt);
	}
}

/**
	Remove os primeiros n caracteres de str
	*/
void chopN(char *str, size_t n){
    size_t len = strlen(str);
    if (n > len)
        return;  // Or: n = len;
    memmove(str, str+n, len - n + 1);
}

//relays 1 message to the other servers
int relaymsg(char *msg, int clock){
    int i;
    
    // format: SMESSAGES\nclock;msg\n\n
    char buffer[256]="SMESSAGES\n", temp[5];
    sprintf(temp, "%d", clock);
    strcat(buffer,temp);
    strcat(buffer,";");
    strcat(buffer,msg);
    strcat(buffer, "\n");
    strcat(buffer, "\n");
    
    // Send message to all connected servers
    for(i=0;i<socketlistsize;i++){
    	if(socketlist[i].fd != -1) {
        	sendTCP(socketlist[i].fd, buffer);
    	}
    }
    return(0);
}

/**
	Sends the string str to fd through TCP

	*/
void sendTCP(int fd, char *str) {
	char* ptr, buffer[4000];
	int nleft, nbytes, nwritten;

	if (fd==-1) return;

	ptr = strcpy(buffer,str);
	nbytes = strlen(ptr);

	nleft=nbytes;
	while(nleft>0){
		nwritten=write(fd,ptr,nleft);
		if(nwritten<=0)exit(1);//error
		nleft-=nwritten;
		ptr+=nwritten;
	}
}

/**	
	Receive a message from other server
	j is the index of the socket to be read in socketlist
	*/
int receiveTCPmsg(MESSAGE *msglist, SOCKET *socketlist, int j){
    char buffer[5140], command[32], *token, temp[8];
    int ret, clock, i;
    
    // read message
    ret=read(socketlist[j].fd,buffer,5140);
    // error
    if(ret==-1){
		printf("read error\n");
        return(1);
    }

    // if no character is read means the server disconnected - close socket and set fd=-1
    if(ret == 0){
    	printf("> Disconnected from %s\n",  inet_ntoa(socketlist[j].addr.sin_addr));
    	close(socketlist[j].fd);
    	socketlist[j].fd = -1;
    	return 0;
    }

    // read command
    buffer[ret] = '\0';
    sscanf(buffer,"%s",command);
    
    if (strcmp(command,"SGET_MESSAGES")==0) {

        //send all messages
        sprintf(buffer,"SMESSAGES\n");
        for(i=0; i<ml_size; i++){

            sprintf(temp,"%d;",msglist[i].time);
        	strcat(buffer, temp);
            strcat(buffer,msglist[i].msg);
            strcat(buffer,"\n");
        }
        strcat(buffer,"\n");

        printf("> Message request from server\n");
        sendTCP(socketlist[j].fd,buffer);

        
    } else if(strcmp(command,"SMESSAGES")==0){
        // receive new message
        printf("> Received message from server\n");
        token=strtok(buffer, "\n");
        token=strtok(NULL, ";");
        sscanf(token, "%d",&clock);
        LC = max(LC, clock) + 1;
        token=strtok(NULL, "\n");
        storeMessage(token, msglist, 1);

    }
    
    return(0);
}

/**
	Envia pedido de mensagens inicial aos servers conectados
	Percorre a lista de servers connectados e envia o pedido e espera pela resposta (TIMEOUT_)
	Se não receber a resposta a tempo considera o outro server inativo e desconecta-se
	Lê e processa a resposta recebida e armazena na lista de mensagens
	*/
int SGetMessages(MESSAGE* msglist){

	char command[32], *token;
	int i = 0, ret, clock, counter=0, maxfd = 0, size;
    struct timeval timeout;
	
    if (socketlistsize > 0) printf("> Retrieving messages\n");

	//Set file descriptor
    fd_set rfds;
    char *buffer = (char*)malloc(10280*sizeof(char));
    // go through all connected servers and send SGET_MESSAGES request
	for(i = 0; i<socketlistsize; i++)
	{
		// send only to connected servers
		if (socketlist[i].fd != -1) {
			// send message request
			sendTCP(socketlist[i].fd, "SGET_MESSAGES\n");

			// use select to generate timeout
	        FD_ZERO(&rfds);
	        
	        // set file descriptor for socket
	        FD_SET(socketlist[i].fd,&rfds);
	        maxfd=socketlist[i].fd;

	        // sets timeout
	        timeout.tv_sec=TIMEOUT_;
	        timeout.tv_usec=0;
	        
	        // Wait for a communication from server
	        counter=select(maxfd+1,&rfds,NULL,NULL,&timeout);
	        // error
	        if(counter==-1) {
	            printf("Select error\n");
	            close(socketlist[i].fd);
	            socketlist[i].fd = -1;
                free(buffer);
	            exit(0);
	        }

	        // timeout: no response - disconnect from server
	        if(counter==0) {
	        	printf("> [No response] Disconnected from %s\n",  inet_ntoa(socketlist[i].addr.sin_addr));
	        	close(socketlist[i].fd);
	        	socketlist[i].fd = -1;
	        }
	        
	        // Received TCP response
	        if(FD_ISSET(socketlist[i].fd,&rfds)){
	            
                
                size=10281;
                ret=read(socketlist[i].fd,buffer,10280);
                if(ret==-1){
                    printf("read error\n");
                    free(buffer);
                    return(1);
                }
                // no bytes read - disconnect from server
                if(ret == 0){
                    printf("> Disconnected from %s\n",  inet_ntoa(socketlist[i].addr.sin_addr));
                    close(socketlist[i].fd);
                    socketlist[i].fd = -1;
                    free(buffer);
                    return 0;
                }
                printf("Received:%d\n", ret);
                
                while (ret==10280) {
                    
                    char *tbuffer = (char*)malloc(size*sizeof(char));
                    strcpy(tbuffer,buffer);
                    ret=read(socketlist[i].fd,tbuffer,10280);
                    if(ret==-1){
                        printf("read error\n");
                        free(tbuffer);
                        free(buffer);
                        return(1);
                    }
                    if(ret == 0){
                        free(tbuffer);
                        break;
                    }
                    size=size+ret;
                    char *buffer = (char*) realloc(&buffer,size*sizeof(char));
                    strcat(buffer,tbuffer);
                    free(tbuffer);
                }
                
                if(size>10280){
                    buffer[size] = '\0';
                } else {
                    buffer[ret] = '\0';
                }
			    sscanf(buffer,"%s",command);

                
			    if(strcmp(command,"SMESSAGES")==0){
			        printf("> Receiving initial message list from %s\n", inet_ntoa(socketlist[i].addr.sin_addr));
			        
                        token=strtok(buffer, "\n");
                        token=strtok(NULL, ";");
                        while (token!=NULL) {
                            sscanf(token, "%d",&clock);
                            
                            LC = max(LC, clock) + 1;
                            token=strtok(NULL, "\n");
                            if(token!=NULL){
                                storeMessage(token, msglist,0);
                            } else {
								 printf(">> Error getting old messages from server\n");
								 break;
							}
                            token=strtok(NULL, ";");
							if(strcmp(token, "\n")==0) break;
                        }
                    free(buffer);
			        return 1;
				}
	        }
		}
	}
    free(buffer);
    return(0);
}


/**
	Prints list of all servers to whom this server is connected to (open TCP session)
	*/
void printConnectedServers() {

	int i;
	
	printf("> Connected servers:\n");
	for (i=0; i<socketlistsize; i++){
		if (socketlist[i].fd != -1){
			printf("\t>IP: %s\t>socket:%d\n", inet_ntoa(socketlist[i].addr.sin_addr), socketlist[i].fd);
		}
	}
}
