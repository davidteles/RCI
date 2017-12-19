/************************************************************************************************
 ******************************************** Projecto RCI **************************************
 ******************************************** 2ºS 2016/17 +**************************************
 *  Cliente                                                                                     *
 *  Principais funções:                                                                         *
 *   - Pedir lista de servidores ao servidor de identidades                                     *
 *   - Receber mensagens via UDP de um servidor usando um protocolo pré definido                *
 *   - Enviar mensagens para o servidor                                                         *
 *   - Introduzir a formatação correcta necessaria para o protocolo usado                       *
 *   - Garantir que a mensagem è enviada para um servidor activo e que esta é recebida          *
 ************************************************************************************************
 ************************************************************************************************
 ********************************* By: David Teles e Lorenço Pato *******************************
 ************************************************************************************************
 ************************************************************************************************/

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#define max(A,B) ((A)>=(B)?(A):(B))

#define CONF_TIMEOUT 4
#define DEFAULTPORT 59000
#define DEFAULTIP "tejo.tecnico.ulisboa.pt"

void run();
int readinput();
int getservers();
int publish(char *msg);
void printserverlist();
int getmessages(int nummsg, int show);

typedef struct SERVER_ {
    char name[100];
    char serverip[100];
	struct in_addr ip;
    int upt, tpt;    
} SERVER;


int fd, listsize=0;
struct hostent *hostptr;
struct sockaddr_in serveraddr, clientaddr;
unsigned int addrlen;
struct SERVER_ serverlist[100];


int main(int argc, char *argv[]){
    //Set Variables
    int sipt=DEFAULTPORT;
    char siip[100]=DEFAULTIP;
    
    //Read arguments
    
    //First argument
    if(argc>1){
        if(strcmp(argv[1],"-i")==0){
            strcpy(siip,argv[2]);

        } else if (strcmp(argv[1],"-p")==0){
        	sscanf(argv[2],"%d", &sipt);
            
        } else {
            printf("First Flag Not Recognized");
            exit(0);
        } 
    }
    
    //Second argument
    if(argc>3){
        if(strcmp(argv[3],"-i")==0){
            strcpy(siip,argv[4]);
            
        } else if (strcmp(argv[3],"-p")==0){
            sscanf(argv[4],"%d", &sipt);
            
        } else {
            printf("Second Flag Not Recognized");
            exit(0);
        }
    }
    //Print the IP and port of the identity server where connection is being estabilished
    printf("Connect to server IP:%s on Port:%d \n", siip,sipt);
    
    //Open Socket 
	fd = socket(AF_INET,SOCK_DGRAM,0);
    
    //Check if socket if correctly opened
	if (fd == -1){
		printf("Socket Error!\n");
		exit(1);	
	}

    //Get identity server info from the server IP
	hostptr=gethostbyname(siip);
    
    //Check if the host was found
	if (hostptr == NULL){
		printf("Gethostbyname Error!\n");
		exit(1);
	}

    //Iniciate sockectadress structure
	memset((void*)&serveraddr,(int)'\0',sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = ((struct in_addr *)(hostptr->h_addr_list[0]))->s_addr;
	serveraddr.sin_port = htons((u_short)sipt);

    //Calculate size of adress
	addrlen = sizeof(serveraddr);
    
    //Main function
    run();
    
    //Close the socket
	close(fd);
    
    //Return to stop the client
    return(0);
}


void run(){
    //Set variables
    int rtn=0;
    
    //Main loop
    do {
        
    
        rtn=readinput();
        
    //If the return from the readinput function was 10, then the user asked to exit the program
    } while(rtn!=10);

}

//Read input given from stdin
int readinput(){
    
    //Set variables
    int temp=0, ret;
    char msg[200], buffer[200], command[20];
    
    //Read the stdin buffer
    while(fgets(buffer,200,stdin)==NULL){}
    
    //Remove comand the comand
    ret=sscanf(buffer,"%s %s\n", command, msg);
    
    
    //Check if the user wants to exit the program
    if(strcmp(command,"exit")==0){
        return(10);
    //Check if the user wants to see the active servers
    } else if (strcmp(command,"show_servers")==0){
        
        //Request new server list
        getservers();
        //Print the server list on the screen
        printserverlist();
        
        return(11);
        
    //Check if the user wants to publish a new message
    } else if (strcmp(command,"publish")==0){
        
        if(strlen(buffer)>140){
            printf("Please, write only messages with less than 140 characters!\n");
            return(1);
        }
        //If the user wrote something in the message section
        if(ret!=1){
            
            //Publish a new message to the message server via UDP
            publish(buffer);
        } else {
            printf("Please write a message to send to the server\n");
        }
        
        return(12);
        
    //Check if the user wants to see the latest messages on the server
    } else if (strcmp(command,"show_latest_messages")==0){
        
        //Remove the number of messages the user wants to see
        sscanf(msg,"%d", &temp);
        //Request messages via UDP form the message server
        getmessages(temp, 1);
        
        return(13);
    
    //If none of the above if's are triggered the command is not valid
    } else {
        
        printf("Command Not Found!\n");
        return(1);
    }
    
    //This return shloud never be hitted
    return(0);
    
}

//Request a list of servers from the identity server
int getservers(){
    
    //Set Variables
    int st, rf,i=0;
    char buffer[5000], *token;
    char msg[20] = "GET_SERVERS";

    //Reset the size of the list
    listsize=0;

    //Send the request to the IDServer
    st = sendto(fd, msg, strlen(msg)+1, 0,(struct sockaddr*)&serveraddr,addrlen);
    
    //Check if the msg was correctly sent
    if (st == -1){
        printf("SendTo error!\n");
        exit(1);
    }
    
    //Get the size of the adress
    addrlen = sizeof(serveraddr);
    
    //Receive the server information from the identity server
    rf = recvfrom(fd, buffer, sizeof(buffer),0,(struct sockaddr*)&serveraddr,&addrlen);
    
    //Check if the data was received correctly
    if (rf == -1){
        printf("Recvfrom error!\n");
        exit(1);
    }
    
    //Manipulate the received data to extrat the server information
    buffer[rf-1]='\0';
    //Extrat the first line
    token=strtok(buffer, "\n");
    token=strtok(NULL, ";");
    //Check if the list is empty
    while(token!=NULL){
        //Save the server name
        strcpy(serverlist[i].name,token);
        token=strtok(NULL, ";");
        //Save the server IP
        strcpy(serverlist[i].serverip,token);
        token=strtok(NULL, ";");
        //Save the UDP port
        sscanf(token, "%d",&(serverlist[i].upt));
        token=strtok(NULL, "\n");
        //Save the TCP port
        sscanf(token, "%d",&(serverlist[i].tpt));
        //Increment the number of servers
        i++;
        token=strtok(NULL, ";");
        
    }
    
    //Set the number of the servers on the list
    listsize=i;
    
    return(0);
    
}

//Publish a new message via UDP to one message server and check if the message was received
int publish(char *buffer){
    
    //Set Variables
    int i, st, maxfd, rtn, counter, rf;
    struct sockaddr_in msgserver;
    struct hostent *msgptr;
    unsigned int addrsize;
    struct timeval timeout;
    char msgret[200], *token, msg[200]="PUBLISH ",  temp[200]="MESSAGES\n";

    //Set file descriptor
    fd_set rfds;
    
    //Get new list of servers
    getservers();
    
    //Clean the user input and get is ready to send to the message server
    buffer[7]=';';
    token=strtok(buffer, ";");
    token=strtok(NULL, "\n");
    strcat(msg, token);

    
    //Run through the server list until the message is published
    for(i=0; i<listsize;i++){
        
        //Get info from the server using the server IP
        msgptr=gethostbyname(serverlist[i].serverip);
        
        //Check if the host was found
        if (msgptr == NULL){
            printf("Gethostbyname Error!\n");
            continue;
        }
        
        
        //Iniciate sockectadress structure
        memset((void*)&msgserver,(int)'\0',sizeof(msgserver));
        msgserver.sin_family = AF_INET;
        msgserver.sin_addr.s_addr = ((struct in_addr *)(msgptr->h_addr_list[0]))->s_addr;
        msgserver.sin_port = htons((u_short)serverlist[i].upt);
        
        //Calclulate the size of the adress
        addrsize = sizeof(msgserver);
        
        //Send the message to the message server
        st = sendto(fd, msg, strlen(msg)+1, 0,(struct sockaddr*)&msgserver,addrsize);
        
        //Check if the seending of the message was a sucess
        if (st == -1){
            printf("SendTo error!\n");
            exit(1);
        }
        
        
        
        //Request the lastest 5 messages to check if the sent message was delievered 
        st = sendto(fd, "GET_MESSAGES 5", strlen("GET_MESSAGES 5")+1, 0,(struct sockaddr*)&msgserver,addrsize);
        
        //Check if the seending of the message was a sucess
        if (st == -1){
            printf("SendTo error!\n");
            exit(1);
        }
        
        
        FD_ZERO(&rfds);
        
        // set file descriptors
        FD_SET(fd,&rfds);
        maxfd=fd;
        // stdin is fd=0
        FD_SET(0,&rfds); maxfd=max(maxfd,0);
        
        /* resets timeout to the refresh rate */
        timeout.tv_sec=CONF_TIMEOUT;
        timeout.tv_usec=0;
        
        //Wait for a communication beeing it via UDP or StdIO
        counter=select(maxfd+1,&rfds,NULL,NULL,&timeout);
        
        //Check if a error as occurred
        if(counter==-1) {
            printf("Select error\n");
            close(fd);
            exit(0);
        }
        
        //Received input on stdin
        if(FD_ISSET(0,&rfds)) {
            rtn=readinput();
            if(rtn==10){
                close(fd);
                exit(0);
            }
        }
        
        //Received UDP communication from fd
        if(FD_ISSET(fd,&rfds)){
            
            rf = recvfrom(fd, msgret, sizeof(msgret),0,(struct sockaddr*)&msgserver,&addrsize);
            
            //Check if the recfrom was sucessfull
            if (rf == -1){
                printf("Recvfrom error!\n");
                exit(1);
            }
            
            //String manipulation to compare
            strcat(temp, token);
            
            //Check if latest message on the server is the one we sent
            if(strstr(msgret, token) != NULL) {
                printf("Enviado para %s\n", serverlist[i].serverip);
                return(1);
            }
            
        }

    }
    

    
    //If the message wasn't published
    printf("There isn't a server online to publish you message!\nPlease, try later\n");
    return(0);
}

//Print the server list on the screen
void printserverlist(){
    //Set of the variables
    int i;
    
    printf("Servers:\n");
    //Go trough the server list and place them on the screen
    for(i=0; i<listsize;i++){
         
         printf("%d %s;%s;%d;%d\n",i+1,serverlist[i].name,serverlist[i].serverip,serverlist[i].upt,serverlist[i].tpt);
     }
}

//Request and display the messages if needed
int getmessages(int nummsg, int show){
    
    //Set Variables
    int i, st, maxfd, rtn, counter, rf;
    struct sockaddr_in msgserver;
    struct hostent *msgptr;
    unsigned int addrsize;
    struct timeval timeout;
    char temp[16],msg[64]="GET_MESSAGES ", buffer[5140];
    
    //ADD the number of messages that the user requested to the message request string
    sprintf(temp, "%d", nummsg);
    strcat(msg, temp);
    
    //Set file descriptor
    fd_set rfds;
    
    //Get new list of servers
    getservers();
    
    //Run through the server list until the message is published
    for(i=0; i<listsize;i++){
        
        //Get info from the server using the server IP
        msgptr=gethostbyname(serverlist[i].serverip);
        
        //Check if the host was found
        if (msgptr == NULL){
            printf("Gethostbyname Error!\n");
            exit(1);
        }

        //Iniciate sockectadress structure
        memset((void*)&msgserver,(int)'\0',sizeof(msgserver));
        msgserver.sin_family = AF_INET;
        msgserver.sin_addr.s_addr = ((struct in_addr *)(msgptr->h_addr_list[0]))->s_addr;
        msgserver.sin_port = htons((u_short)serverlist[i].upt);
        
        //Calclulate the size of the adress
        addrsize = sizeof(msgserver);
        
        //Send the message to the message server
        st = sendto(fd, msg, strlen(msg)+1, 0,(struct sockaddr*)&msgserver,addrsize);
        if (st == -1) {
			printf("SendTo error\n");
			exit(0);
		}
        
        
        FD_ZERO(&rfds);
        
        // set file descriptors
        FD_SET(fd,&rfds);
        maxfd=fd;
        // stdin is fd=0
        FD_SET(0,&rfds); maxfd=max(maxfd,0);
        
        /* resets timeout to the refresh rate */
        timeout.tv_sec=1;
        timeout.tv_usec=0;
        
        //Wait for a communication beeing it via UDP or StdIO
        counter=select(maxfd+1,&rfds,NULL,NULL,&timeout);
        
        //Check if a error as occurred
        if(counter==-1) {
            printf("Select error\n");
            close(fd);
            exit(0);
        }
        
        //Received input on stdin
        if(FD_ISSET(0,&rfds)) {
            rtn=readinput();
            if(rtn==10){
                close(fd);
                exit(0);
            }
        }
        
        //Received UDP communication from fd
        if(FD_ISSET(fd,&rfds)){
            
            //Get the messages send by the server
            rf = recvfrom(fd, buffer, sizeof(buffer),0,(struct sockaddr*)&msgserver,&addrsize);
            
            //Check if the recfrom was sucessfull
            if (rf == -1){
                printf("Recvfrom error!\n");
                exit(1);
            }
            
            //If the fuction was called to display the messages show them on the display
            if(show==1){
                printf("%s", buffer);
                return(1);
            }
            
        }
    }
    
    return(0);
}
