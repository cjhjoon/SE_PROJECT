#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFSIZE		1024
#define MAX_USER	5

void Start_Chatting(int user_sock, int port);

int main(int argc, char* argv[]) {
  int user_sock, proxyServer_sock;
  struct sockaddr_in user_addr;
  const int loginServer_port = 40512;
  int myport;
  struct hostent *loginServer_host;
  char buf[BUFSIZE];

  if((user_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
	perror("user socket");
	exit(1);
  }


  if((loginServer_host = gethostbyname("localhost")) == 0) {
	fprintf(stderr, "%d: unknown host\n", loginServer_port);
	exit(1);
  }
  memset((void*)&user_addr, 0, sizeof(user_addr));
  user_addr.sin_family = AF_INET;
  memcpy((void*)&user_addr.sin_addr, loginServer_host->h_addr, loginServer_host->h_length);
  user_addr.sin_port = htons((u_short)loginServer_port);


  if(connect(user_sock, (struct sockaddr*)&user_addr, sizeof(user_addr)) < 0) {
	printf("connect\n");
	(void) close(user_sock);
	fprintf(stderr, "connect");
	exit(1);
  }

  int bytesread = read(user_sock, buf, BUFSIZE);
   buf[bytesread] = '\0';

  myport = atoi(buf);
  printf("My Port Number : %d\n",myport);
  printf("**********************************************************************\n");
  printf("@login   you can login at login-server\n");
  printf("@invite  if you success to login, you can invite user from user list\n");
  printf("@quit    you can quit the program\n");
  printf("**********************************************************************\n");
  buf[0] = '\0';
  
  sleep(2);
  Start_Chatting(user_sock, myport);

  (void) close(user_sock);
} //end main

void Start_Chatting(int user_sock, int portnum)
{

	//var list
	int proxyServer_sock;
	int userAry[MAX_USER];
	int numUser = 0;
	int i;
	int isProxy = 0;
	int login_flag = 0;

	int peertcpSocket = -1;
	int clnt_len;
	struct sockaddr_in proxyServer_addr;
	struct hostent *loginServer_host;
	char command[BUFSIZE];
	char message[BUFSIZE];
	fd_set reads, temps;

	char userIDLogin[BUFSIZE];
	char userPWLogin[BUFSIZE];
	char userIDInvite[BUFSIZE];
	
	proxyServer_sock = socket(PF_INET, SOCK_STREAM, 0);
	proxyServer_addr.sin_family = AF_INET;
	proxyServer_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	proxyServer_addr.sin_port = htons(portnum);

	if (bind(proxyServer_sock, (struct sockaddr *)&proxyServer_addr, sizeof proxyServer_addr) < 0) {
		perror("bind2");
		exit(1);
	}
	
	if (listen(proxyServer_sock, MAX_USER) < 0) {
		perror("listen");
		exit(1);
	}

	char YorN;
	FD_ZERO(&reads);
	FD_SET(fileno(stdin), &reads);
	FD_SET(proxyServer_sock, &reads);
	FD_SET(user_sock, &reads);
	

	int isChatting = 0;

	while(1)
	{
		int port;
		struct sockaddr_in clnt_addr;
		temps = reads;
	
		select(FD_SETSIZE, &temps, 0, 0, NULL);

		if ( FD_ISSET(proxyServer_sock, &temps ) )
		{
			if(numUser >= MAX_USER) {
				printf("too much user connect!\n");
				exit(0);
			}
			clnt_len = sizeof(clnt_addr);
			userAry[numUser] = accept(proxyServer_sock, (struct sockaddr *)&clnt_addr, &clnt_len);
			FD_SET(userAry[numUser], &reads);
			printf("connection from host %s, port %d, socket %d \n", inet_ntoa(clnt_addr.sin_addr), port, userAry[numUser]);
			isChatting = 1;
			isProxy = 1;
			FD_CLR(proxyServer_sock, &temps);
			numUser++;
		}
		else if ( FD_ISSET( user_sock, &temps ) )
		{
			char *Proxy_ID;
			char *Proxy_Addr;
			char *Proxy_Port;
			char temp[BUFSIZE];
			char *ptr;
				
			command[0] = '\0';
			
			if(read(user_sock, command, BUFSIZE) < 0)
			{
				printf("recv error");
				exit(1);
			}
			if(strncmp(command, "PROXY\n", 6) == 0)
			{
				isProxy++;
			}
			if(strncmp(command, "INVITE\n", 7) == 0)
			{
				int port;
				int c;
				memmove(temp, command, BUFSIZE);

				ptr = strtok( temp, ": \n");
				Proxy_ID = strtok(NULL, "\n: ");
				Proxy_ID = strtok(NULL, " :\n");
				Proxy_Addr = strtok(NULL, "\n: ");
				Proxy_Addr = strtok(NULL, "\n :");
				Proxy_Port = strtok(NULL, "\n :");
				Proxy_Port = strtok(NULL, "\n :");
				port = atoi(Proxy_Port);

				//ask connection
				while(1)
				{
					printf("\'%s\' invite you! Do you accept invitation? [y/n]\n", Proxy_ID);
					while((c = getchar()) != '\n' && c != EOF);
					scanf("%c", &YorN);
				
					if(YorN == 'y' || YorN == 'Y') {
						//Proxy_Client try to connnet Proxy_server
						peertcpSocket = tcpConnect(port, Proxy_Addr);
					
						FD_SET(peertcpSocket, &reads);
	
						isChatting = 1;

					//send message to server for chatting
						memset((char*)message, '\0', sizeof(message));
						strcat(message, "USER\nType: invite\nFrom: ");
						strcat(message, Proxy_ID);
						strcat(message, "\nTo: ");
						strcat(message, userIDLogin);
						if(write(user_sock, message, strlen(message)) < 0)
						{
							printf("request is failed.\n");
							exit(1);
						}
						memset((char*)message, '\0', sizeof(message));
						break;
					}//YorN Yes
					else if(YorN == 'n' || YorN == 'N') {
						printf("You refused the invitation\n");
						break;
					}//YorN No
					else{
						printf("Wrong Input! Try again\n");
						continue;
					}//YorN else

				}//Question While
			}//INVITE command

			if(strncmp(command, "RESET\n", 6) == 0)
			{
				int port;
				int c;
				memmove(temp, command, BUFSIZE);

				ptr = strtok( temp, ": \n");
				Proxy_ID = strtok(NULL, "\n: ");
				Proxy_ID = strtok(NULL, " :\n");
				Proxy_Addr = strtok(NULL, "\n: ");
				Proxy_Addr = strtok(NULL, "\n :");
				Proxy_Port = strtok(NULL, "\n :");
				Proxy_Port = strtok(NULL, "\n :");
				port = atoi(Proxy_Port);

				close(peertcpSocket);
				printf("IP : %s  PORT : %d\n",Proxy_Addr, port);
				port = port/10;

				peertcpSocket = tcpConnect(port, Proxy_Addr);				
				FD_SET(peertcpSocket, &reads);
				isChatting = 1;

				
			}//RESET command
		}
		else if ( FD_ISSET( fileno(stdin), &temps ) )
		{
			fgets( command, sizeof (command), stdin );
			FD_CLR(fileno(stdin), &temps);

			if(strncmp(command, "@login", 6) == 0)
			{
				if (login_flag == 1) {
					printf("You already login!\n");
					continue;
				}
				memset((char*)userIDLogin, '\0', sizeof(userIDLogin));
				memset((char*)userPWLogin, '\0', sizeof(userPWLogin));
				memset((char*)message, '\0', sizeof(message));
				strcat(message, "LOGIN\n");
				strcat(message, "ID: ");
				printf("ID: ");
				scanf("%s", userIDLogin);
				strcat(message, userIDLogin);
				strcat(message, "\nPW: ");
				printf("PW: ");
				scanf("%s", userPWLogin);
				strcat(message, userPWLogin);
				if(write(user_sock, message, strlen(message)) < 0)
				{
					printf("request is failed.\n");
					exit(1);
				}
				memset((char*)message, '\0', sizeof(message));	
				if(read(user_sock, message, BUFSIZE) < 0)
				{
					printf("recv error");
					exit(1);
				}
				printf("%s\n", message);

				if(!strncmp(message, "OK", 2)) {
					memset((char*)message, '\0', sizeof(message));	
					strcat(message, "LIST");
				
					if(write(user_sock, message, strlen(message)) < 0)
					{
						printf("request is failed.\n");
						exit(1);
					}
					memset((char*)message, '\0', sizeof(message));	
					if(read(user_sock, message, BUFSIZE) < 0)
					{
						printf("recv error");
						exit(1);
					}
					printf("%s\n", message);
				}

				memset((char*)command, '\0', sizeof(command));	
				login_flag++;
			}
			else if ( strncmp(command, "@invite", 7) == 0 )
			{
				//input @login first.
				if (login_flag == 0) {
					printf("Login First!\n");
					continue;
				}
				char Info[1024];
				memset((char*)message, '\0', sizeof(message));
				strcat(message, "LIST");
				if(write(user_sock, message, strlen(message)) < 0)
				{
					printf("Request is failed. - LIST\n");
					exit(1);
				}
				memset((char*)message, '\0', sizeof(message));	
				printf("Choose user for invite!\n");

				if(read(user_sock, message, BUFSIZE) < 0)
				{
					printf("recv error");
					exit(1);
				}
				printf("%s\n", message);
				memset((char*)command, '\0', sizeof(command));	


				//now can invite~ (in list)
				memset((char*)message, '\0', sizeof(message));
				scanf("%s", userIDInvite);

				memset((char*)message, '\0', sizeof(message));
				strcat(message, "INVITE\n");
				strcat(message, "From: ");
				strcat(message, userIDLogin);
				strcat(message, "\nTo: ");
				strcat(message, userIDInvite);

				if(write(user_sock, message, strlen(message)) < 0)
				{
					printf("request is failed.\n");
					exit(1);
				}
				memset((char*)message, '\0', sizeof(message));	
			}
/****delete bye
			else if ( strncmp(command, "@bye", 4) == 0 )
			{			
				//Chatting first.
				if (isChatting == 0){
					printf("Chatting first!\n");
					continue;
				}
				printf("Do you want to exit the room? [y/n]\n");
				scanf("%c", &YorN);
				if(YorN == 'n' || YorN == 'N')
					continue;

				if(isChatting && !isProxy) {
					memset((char *)message, '\0', sizeof(message));
					strcat(message, "@bye");

					if(write(peertcpSocket, message, strlen(message)) < 0)
					{
						printf("Request is failed.\n");
						exit(1);
					}
					message[0] = '\0';
				} else if(isChatting) { //quit by Proxy 
					int idx;
					memset((char *)message, '\0', sizeof(message));
					strcat(message, "@bye");

				   	for(idx=0; idx<numUser; idx++) {
						if(write(userAry[idx], message, strlen(message)) < 0)
						{
							printf("Request is failed.\n");
							exit(1);
						}
				   	}
				}

				memset((char *)message, '\0', sizeof(message));
				strcat(message, "USER\n");
				strcat(message, "Type: bye\n");
				strcat(message, "From: ");
				strcat(message, userIDLogin);
				strcat(message, "\nProxy: ");
				if(isProxy)
					strcat(message, "y");
				else
					strcat(message, "n");


				if(write(user_sock, message, strlen(message)) < 0)
				{
					printf("Request is failed.\n");
					exit(1);
				}
				printf( "Connection Closed\n");
				close(peertcpSocket);
				peertcpSocket = -1;
			}
delete bye***/
			else if ( strncmp(command, "@quit", 5) == 0 )
			{	
				printf("Do you want to quit program? [y/n]\n");
				scanf("%c", &YorN);
				if(YorN == 'n' || YorN == 'N')
					continue;

				if(isChatting && !isProxy) {
					memset((char *)message, '\0', sizeof(message));
					strcat(message, "@quit");

					if(write(peertcpSocket, message, strlen(message)) < 0)
					{
						printf("Request is failed.\n");
						exit(1);
					}
					message[0] = '\0';
				} else if(isChatting) { //quit by Proxy 
					int idx;
					memset((char *)message, '\0', sizeof(message));
					strcat(message, "@quit");

				   	for(idx=0; idx<numUser; idx++) {
						if(write(userAry[idx], message, strlen(message)) < 0)
						{
							printf("Request is failed.\n");
							exit(1);
						}
				   	}
				}

				memset((char *)message, '\0', sizeof(message));
				strcat(message, "USER\n");
				strcat(message, "Type: quit\n");
				strcat(message, "From: ");
				strcat(message, userIDLogin);
				strcat(message, "\nProxy: ");
				if(isProxy)
					strcat(message, "y");
				else
					strcat(message, "n");


				if(write(user_sock, message, strlen(message)) < 0)
				{
					printf("Request is failed.\n");
					exit(1);
				}
				printf( "Connection Closed\n");
				close(peertcpSocket);
				peertcpSocket = -1;
				return;
			}
			else if(isChatting)
			{
				int j;
				char message[BUFSIZE];

				if(isChatting == 1) {
					isChatting++;
					continue;
				}

				//write message to proxy user
				memset((char *)message, '\0', sizeof(message));
				strcat(message, userIDLogin);
				strcat(message, ": ");
				strcat(message, command);

				if(isProxy) {
				   for(j=0; j<numUser; j++) {
					if (write(userAry[j], message, strlen(message)) < 0) 
					{
						perror("write");
						exit(1);
					}
				   }
				} else {
				   if(write(peertcpSocket, message, strlen(message)) < 0) {
					perror("write");
					exit(1);
				   }

				}
			}
		}
		else
//read message from another user
		{
			if(isProxy) {
			   for(i=0; i<numUser; i++) {
			   	if(FD_ISSET(userAry[i], &temps)) {
					int bytesread = read(userAry[i], command, BUFSIZE);
					int j;
					command[bytesread] = '\0';

					if(!strcmp(command, "@quit") || !strcmp(command, "@bye")) {
						int idx;
						FD_CLR(userAry[i], &reads);
						(void) close(userAry[i]);
						
						for(idx=i; idx<numUser-1; idx++)
							userAry[idx] = userAry[idx+1];

						i--;
						numUser--;
						if(!numUser) {
							isProxy = 0;
							memset((char *)message, '\0', sizeof(message));
							strcat(message, "USER\nType: state");

							if(write(user_sock, message, strlen(message)) < 0) {
								perror("write");
								exit(1);
							}
						}
						continue;
					}
					printf( "%s\n", command );

					//transport message for all user in proxy
					for(j=0; j<numUser; j++) {
						if (write(userAry[j], command, strlen(command)) < 0) 
						{
							perror("write");
							exit(1);
						}
					}
					command[0] = '\0';
			   	}
			   }
			} else {
				if(FD_ISSET(peertcpSocket, &temps)) {
					int bytesread = read(peertcpSocket, command, BUFSIZE);
					command[bytesread] = '\0';

					if(!strcmp(command, "@quit")) {
						FD_CLR(peertcpSocket, &reads);
						(void) close(peertcpSocket);
						isChatting = 0;
						continue;
					}
					printf( "%s\n", command );
					command[0] = '\0';
				}	
			}
			if(i == numUser) continue;

			memset((char *)command, '\0', sizeof(command));
			if (clnt_len<0) {
				perror("read");
				exit(1);
				/* fall through */
			}
			if(clnt_len <=0)
			{ 
				FD_CLR(peertcpSocket, &reads);
				close(peertcpSocket);
				printf("Close Socket %d\n", peertcpSocket);
			} 
		}
	}
}


int tcpConnect(int port, char* IP)
{
	struct sockaddr_in tcpClient_addr;
	struct hostent* host;
	int sock;

	if ( ( sock = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ) < 0){
		perror("socket");
		/* fall through */
	}

	if((host = gethostbyname(IP)) == 0) {
	fprintf(stderr, "%d: unknown host\n", port);
	exit(1);
	}
	memset(&tcpClient_addr, 0, sizeof(tcpClient_addr));
	tcpClient_addr.sin_family=AF_INET;
	memcpy((void *) &tcpClient_addr.sin_addr, host->h_addr, host->h_length);
	tcpClient_addr.sin_port=htons(port);

	if( connect( sock, (struct sockaddr *)&tcpClient_addr, sizeof(tcpClient_addr) ) < 0 ){
		perror("connect");
		exit(1);		
		/* fall through */
	}
	
	printf("connection is made successfully\n");

	return sock;
}
