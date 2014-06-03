// 20123381 Hyesu Shin

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

//Maximum number of users is 100 for login server
#define MAX_USER	100
#define BUFSIZE		1024
#define PORTNUM		10000 
//Maximum length of user name is 30 characters
#define MAX_USER_STRLEN	30

void badRequestFormat();

char* stateStr[] = {"login", "proxy"};

int main(int argc, char* argv[]) {
	//var list for login server
	int loginServer_sock;
	struct sockaddr_in loginServer_addr;
	const int loginServer_port = 40512;
	fd_set reads, temp;
	int fd_max, nFound;

	//file for user id & password 
	FILE *db;
	//if users success login process, their id is stored at these array
	char connectedUser[MAX_USER_STRLEN][MAX_USER];
	int userPort[MAX_USER];
	int userState[MAX_USER];
	char userIP[MAX_USER_STRLEN][MAX_USER];
	int proxy = 0;


	//var list for user
	int addrlen, user_sock[MAX_USER]; 
	struct sockaddr_in user_addr[MAX_USER];
	int numUser = 0;
	char buf[BUFSIZE];

	int i, j, k;
	int ProxyOfClient = 0;

	// make login server socket
	memset((void*)&loginServer_addr, 0, sizeof(loginServer_addr));
	loginServer_sock = socket(PF_INET, SOCK_STREAM, 0);
	loginServer_addr.sin_family = AF_INET;
	loginServer_addr.sin_addr.s_addr = INADDR_ANY;
	loginServer_addr.sin_port = htons((u_short)loginServer_port);


	// bind
	if(bind(loginServer_sock, (struct sockaddr*)&loginServer_addr, sizeof(loginServer_addr)) < 0) {
		perror("bind");
		exit(1);
	}


	// listen. print out current number of users
	printf("\nnumUser: %d, Waiting for connection...\n", numUser);
	if(listen(loginServer_sock, SOMAXCONN) < 0) {
		perror("listen");
		exit(1);
	}


	FD_ZERO(&reads);
	FD_SET(loginServer_sock, &reads);

	while(1) {
		temp = reads;
		nFound = select(FD_SETSIZE, &temp, (fd_set*)0, (fd_set*)0, NULL);
		printf("\nnumUser: %d, Waiting for connection...\n", numUser);

		//if there is trial of connection by users
		if(FD_ISSET(loginServer_sock, &temp)) {
			addrlen = sizeof(user_addr);
			//user socket that tries to connect is stored at user_sock array
			user_sock[numUser] = accept(loginServer_sock, (struct sockaddr*)&user_addr[numUser], &addrlen);
			if(user_sock[numUser] < 0) {
				perror("accept");
				exit(1);
			}

			printf("\nConnection : Host IP %s, Port %d, socket %d\n",
				inet_ntoa(user_addr[numUser].sin_addr), ntohs(user_addr[numUser].sin_port), user_sock[numUser]);

			//store the information of connected user
			strcpy(buf, "\0");
			sprintf(buf, "%d",PORTNUM+numUser);
			userPort[numUser] = PORTNUM+numUser;
			userState[numUser] = -2;	//state for user who is not login yet
			strcpy(userIP[numUser], "\0");
			strcat(userIP[numUser], inet_ntoa(user_addr[numUser].sin_addr));

			//send port number for proxy user so that the user could be a server for another user.
			if(write(user_sock[numUser], buf, strlen(buf)) < 0) {
				perror("write");
				exit(1);
			}

			//setting file descriptor for take message from connected user
			FD_SET(user_sock[numUser], &reads);
			numUser++;
		} //end_if serversock
		else {
			//about all user, check whether there is message from user or not
			for(i=0; i<numUser; i++) {
				int isEndProcess = 0;

				//there is message from user
				if(FD_ISSET(user_sock[i], &temp)) {
					//var list for message
					int bytesread = read(user_sock[i], buf, BUFSIZE);
					char *tempStr = (char*) malloc(BUFSIZE), *bufTok;
					char userid[MAX_USER_STRLEN], password[MAX_USER_STRLEN], tempTok[MAX_USER_STRLEN];

					buf[bytesread] = '\0';

					printf("***************************************************\n");
					printf("message from user %d\n", i);
					printf("%s\n", buf);
					printf("***************************************************\n");

					//now buffer is ready. need to tokenize buffer and get message
					memcpy(tempStr, buf, strlen(buf)+1);

					bufTok = strtok(tempStr, " :\n");
					if(bufTok == NULL) {
						badRequestFormat();
						continue;
					}
					//if the message is started with "LOGIN", the message is for LOGIN function
					if(!strcmp(bufTok, "LOGIN")) {
						printf("\nget LOGIN message from %d user\n", i);
						bufTok = strtok(NULL, " :\n");

						//if user id is not sent, login server send ERROR message to user program
						if(bufTok == NULL) {
							strcpy(buf, "\0");
							strcat(buf, "ERROR wrong password\n");

							if(write(user_sock[i], buf, strlen(buf)) < 0) {
								perror("write");
								exit(1);
							}
							badRequestFormat();
							continue;
						} else if(!strcmp(bufTok, "ID")) {
							bufTok = strtok(NULL, " :\n");	//now 'bufTok' is userid
							strcpy(connectedUser[i], "\0");	//store userid at connectedUser array
							strcat(connectedUser[i], bufTok);
							printf("user %d: %s\n", i, connectedUser[i]);

							//search userid in data.db and check password
							db = fopen("data.db", "a+");
							while(!feof(db)) {
								fscanf(db, "%s", userid);		//'userid' is user id stored in data.db
								fscanf(db, "%s", password);		//'password' is password stored in data.db

								if(!strcmp(bufTok, userid)) {
									isEndProcess = 1;
									// detect userid and check password
									bufTok = strtok(NULL, " :\n");
									if(!strcmp(bufTok, "PW")) {
										bufTok = strtok(NULL, " :\n");	//bufTok = PW from user program
										if(!strcmp(bufTok, password)) {
											//need to send message of OK to client
											strcpy(buf, "\0");
											strcat(buf, "OK login success\n");
											userState[i] = -1;
										} else {
											numUser--;
											//need to send message of BAD to client
											strcpy(buf, "\0");
											strcat(buf, "ERROR wrong password\n");
										}

										if(write(user_sock[i], buf, strlen(buf)) < 0) {
											perror("write");
											exit(1);
										}
									}
									break;
								}
							}
							if(isEndProcess) {
								close(db);
								continue;
							}

							// cannot find userid in data.db. So need to add userid in data.db
							fprintf(db, "%s ", bufTok); 

							bufTok = strtok(NULL, " :\n");
							if(!strcmp(bufTok, "PW")) {
								bufTok = strtok(NULL, " :\n");
								fprintf(db, "%s ", bufTok); 
							}

							//need to send message of OK to client
							strcpy(buf, "\0");
							strcat(buf, "OK login success\n");
							userState[i] = -1;

							if(write(user_sock[i], buf, strlen(buf)) < 0) {
								perror("write");
								exit(1);
							}

							close(db);
						}
					}
					//if there is message for LIST by user program
					else if(!strcmp(bufTok, "LIST")) {
						printf("\nget LIST message from %d user\n", i);
						buf[0] = '\0';

						for(j=0; j<numUser; j++) {
							if(userState[j] > -2) {
								sprintf(tempTok, "%s\t%d", connectedUser[j], userState[j]);
								strcat(buf, tempTok);
								strcat(buf, "(");
								if(userState[j] == -1)
									strcat(buf, stateStr[0]);
								else
									strcat(buf, stateStr[1]);
								strcat(buf, ")\n");
							}
						}
						strcat(buf, "> ");

						if(write(user_sock[i], buf, strlen(buf)) < 0) {
							perror("write");
							exit(1);
						}
					} 
					//if there is message for update user state by user program
					else if(!strcmp(bufTok, "USER")) {
						bufTok = strtok(NULL, " :\n");
						if(bufTok == NULL) {
							badRequestFormat();
							continue;
						}
						//check the update type
						else if(!strcmp(bufTok, "Type")) {
							bufTok = strtok(NULL, " :\n");
							if(bufTok == NULL) {
								badRequestFormat();
								continue;
							}
							//if update type is "invite"
							else if(!strcmp(bufTok, "invite")) {
								printf("\nget INVITE message from %d user\n", i);
								bufTok = strtok(NULL, " :\n");
								if(!strcmp(bufTok, "From")) {
									bufTok = strtok(NULL, " :\n");	
									userid[0] = '\0';
									strcpy(userid, bufTok);		//now 'userid' is userid inviting another user

									bufTok = strtok(NULL, " :\n");
									if(!strcmp(bufTok, "To")) {
										bufTok = strtok(NULL, " :\n");
										tempTok[0] = '\0';
										strcpy(tempTok, bufTok);	//now 'tempTok' is invited userid

										//update user state who is inviting another user
										//if the user is proxy already, do not need to update user state
										for(j=0; j<numUser; j++) {
											if(!strcmp(connectedUser[j], userid)) { 
												if(userState[j] < 0)
													userState[j] = proxy++;
												break;
											}
										}	

										//update user state who is invited by proxy user
										for(k=0; k<numUser; k++) {
											if(!strcmp(connectedUser[k], tempTok)) {
												userState[k] = userState[j];
												break;
											}
										}
									} else {
										badRequestFormat();
										continue;
									}
								} else {
									badRequestFormat();
									continue;
								}
							} 
							//if update type is "invite"
							else if(!strcmp(bufTok, "quit")) {
								printf("\nget QUIT message from %d user\n", i);

								bufTok = strtok(NULL, " :\n");
								if(!strcmp(bufTok, "From")) {
									bufTok = strtok(NULL, " :\n");
									userid[0] = '\0';
									strcpy(userid, bufTok);	//now 'userid' is userid that wants to quit program

									bufTok = strtok(NULL, " :\n");
									bufTok = strtok(NULL, " :\n");

									//add on
									if(ProxyOfClient == user_sock[i]) { //if proxy user wants to quit program
										int number = 0;
										for(j=0; j<numUser; j++){
											if((ProxyOfClient != user_sock[j]) && (user_sock[j] > 0)) {
												ProxyOfClient = user_sock[j];
												number = j;
												strcpy(buf, "\0");
												strcat(buf, "PROXY\n");
												if(write(user_sock[j], buf, strlen(buf)) < 0) {
													perror("write");
													exit(1);
												}//end of write
												break;
											}
											else
												continue;
										}
										for(j=0; j<numUser; j++){	//if not new proxy, receive connection message
											if((ProxyOfClient != user_sock[j]) && (user_sock[i] != user_sock[j]) && (userState[i] == userState[j])) {
												strcpy(buf, "\0");
												strcat(buf, "RESET\nFrom: ");
												strcat(buf, userid);
												strcat(buf, "\nIP: ");
												strcat(buf, userIP[number]);

												strcat(buf, "\nPort: ");
												strcpy(tempStr, "\0");
												sprintf(tempStr, "%d", userPort[number]);
												strcat(buf, tempStr);

												if(write(user_sock[j], buf, strlen(buf)) < 0) {
													perror("write");
													exit(1);
												}//end of write
											}//end of detection if
											else
												continue;
										}//end of for loop
									}//end of if quit == proxy

									if(!strcmp(bufTok, "y")) {
										for(j=0; j<numUser; j++) {
											if(userState[i] == userState[j])
												userState[j] = -1;
										}
									}

									//close connected user socket that wants to quit program
									FD_CLR(user_sock[i], &reads);
									(void) close(user_sock[i]);
									strcpy(connectedUser[i], "\0");
									strcpy(userIP[i], "\0");

									//because the data structure for user is array,
									//if user quit program, information in all array needs to be modified
									for(j=i+1; j<numUser; j++) {
										user_sock[j-1] = user_sock[j];
										strcpy(connectedUser[j-1], connectedUser[j]);
										strcpy(connectedUser[j], "\0");
										userPort[j-1] = userPort[j];
										userState[j-1] = userState[j];
										strcpy(userIP[j-1], userIP[j]);
										strcpy(userIP[j-1], "\0");
									}
									numUser--;
								}
							} else if(!strcmp(bufTok, "state")) {
								userState[i] = -1;
							}
						} // end_User message
					} 

					//if there is message for invite another user from proxy user
					else if(!strcmp(bufTok, "INVITE")) {
						bufTok = strtok(NULL, " :\n");
						if(bufTok == NULL) {
							badRequestFormat();
							continue;
						}
						else if(!strcmp(bufTok, "From")) {
							bufTok = strtok(NULL, " :\n");
							strcpy(userid, "\0");
							strcpy(userid, bufTok);		//proxy userid

							//need to detect proxy user
							for(k=0; k<numUser; k++) {
								if(!strcmp(connectedUser[k], userid))
									break;
							} //now k = index of proxy user

							bufTok = strtok(NULL, " :\n");
							if(bufTok == NULL) {
								badRequestFormat();
								continue;
							} else if(!strcmp(bufTok, "To")) {
								bufTok = strtok(NULL, " :\n");
								strcpy(tempTok, "\0");
								strcpy(tempTok, bufTok);	//invited userid

								ProxyOfClient = user_sock[i];

								for(j=0; j<numUser; j++) {
									if(!strcmp(connectedUser[j], tempTok)) {
										//inform proxy user id, IP address, port to invited user
										// so that the invited user could agree the invitation and try connect to proxy user
										strcpy(buf, "\0");
										strcat(buf, "INVITE\nFrom: ");
										strcat(buf, userid);
										strcat(buf, "\nIP: ");
										strcat(buf, userIP[k]);

										strcat(buf, "\nPort: ");
										strcpy(tempStr, "\0");
										sprintf(tempStr, "%d", userPort[k]);
										strcat(buf, tempStr);

										if(write(user_sock[j], buf, strlen(buf)) < 0) {
											perror("write");
											exit(1);
										}
										break;
									}
								} //end_for j-loop(detect invited user & write message)
							}
						}
					} //end_INVITE command

					buf[0] = '\0';
					free(tempStr);
				} //end message from client
			} //end for-loop for client
		} //end if

	}//end whild(1)
}//end main


void badRequestFormat()
{
	printf("bad request format!\n");
	exit(0);
}
