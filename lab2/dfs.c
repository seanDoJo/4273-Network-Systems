#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>

#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <regex.h>
#include <dirent.h>
int hostsock(const char* portnum, int waitLen);
void handleRequest(void* ptr);
void setParameters(void);
int getFileName(char* packet, char fileName[]);
int userIsValid(char* packet, char* username);
int getRequestType(char* packeto);
void sendFile(int sock, char* workingdirectory, char* filename);
void sendList(int sock, char* directory, char* uname);
void sendError(int sock, char* msg);
void sendClosingAck(int sock);
void receiveFile(int sock, char* workingdirectory, char* filename);
struct targs {
	int newSock;
};
int numUsers = 0;
char userpass[20][2048];
char portnum[32];
char docroot[1024];

int main(int argc, char* argv[]){
	
	if(argc < 3){
		printf("need to specify document root and port number!\nUsage: ./dfs DOCROOT PORTNUM\n");
		exit(1);
	}
	if(argv[1][strlen(argv[1])-1] != '/')strcat(argv[1], "/");
	strcpy(docroot,argv[1]);
	printf("%s\n", docroot);
	strcpy(portnum,argv[2]);
	setParameters();
	struct sockaddr_in fsin;
	int hSock, rc;

	hSock = hostsock(portnum, 32);
	unsigned int fsize = sizeof(fsin);
	while(1){
		int incoming;

		incoming = accept(hSock, (struct sockaddr*)&fsin, &fsize);

		struct targs* newargs = malloc(sizeof(struct targs));
		newargs -> newSock = incoming;
		pthread_t newConnection;
		
		//spawn a new thread on receipt of a new connection
		rc = pthread_create(&newConnection, NULL, (void*)handleRequest, (void*)(newargs));
		if(rc < 0){
			printf("pthread failed!\n");
			//sendServerError(incoming);
			close(incoming);
			free(newargs);
		}
	}
}

int hostsock(const char* portnum, int waitLen){
	struct sockaddr_in sin;
	int sock;

	//initialize sin
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port=htons((unsigned short)atoi(portnum));

	//create the socket
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	//bind the socket
	if(bind(sock, (struct sockaddr*)&sin, sizeof(sin)) < 0){
		printf("Couldn't bind to port %s\nExiting!\n", portnum);
		exit(1);
	}

	//listen on socket
	if(listen(sock, waitLen) < 0){
		printf("Couldn't listen on socket\n");
		exit(1);
	}
	printf("Listening on %s...\n", portnum);

	return sock;
}

void handleRequest(void* ptr){
	struct targs* args = ptr;
	int sock, copied;
	time_t seconds;
	char buf[4096];
	char packet[8192];
	int keepalive = 1;

	strcpy(packet, "");
	sock = args -> newSock;

	//read in data
	//initial blocking receive to ensure we connect
	recv(sock, buf, sizeof(buf), 0);
	int x;
	strcat(packet, buf);
	for(x=0;x<4096;x++)buf[x] = 0;
	do{
		//read whatever is on the socket fully
		while(copied = recv(sock, buf, sizeof(buf), MSG_DONTWAIT) > 0){
			int i;
			strcat(packet, buf);
			for(i=0;i<4096;i++)buf[i] = 0;
		}
		//extract data from packet
		if(strlen(packet) > 0){
			//update our timeout
			seconds = time(NULL);
			//Check for valid username and password
			char uname[2048];
			if(userIsValid(packet, uname)){
				//shift to user's document root
				char workingdirectory[2048];
				strcpy(workingdirectory, docroot);
				strcat(workingdirectory, uname);
				strcat(workingdirectory, "/");
				mkdir(workingdirectory, 0755);
				
				//determine what the user wants us to do
				int request = getRequestType(packet);
				if (request == 0){
					//handle GET
					char filename[2048];
					getFileName(packet, filename);
					sendFile(sock, workingdirectory, filename);
					keepalive = 0;
				} else if(request == 1){
					//handle PUT
					char filename[2048];
					getFileName(packet, filename);
					receiveFile(sock, workingdirectory, filename);
					keepalive = 0;
				} else if(request == 2){
					//handle LIST
					sendList(sock, workingdirectory, uname);
				}else {
					//handle ERROR
				}
			} else {
				//send incorrect user
				sendError(sock, "Invalid Username/Password. Please try again.\n");	
			}
		}
		int i;
		for(i=0;i<sizeof(packet);i++)packet[i] = 0;
		strcpy(packet, "");
		if(copied == 0)break;
	}while(((time(NULL)) - (seconds) < 1) && keepalive);
	close(sock);	
	free(args);
	pthread_exit((void*)(0));
}

int getFileName(char* packeto, char fileName[]){
	char packet[8192];
	strcpy(packet, packeto);
	char* token = strtok(packet, "\n");
	char startLine[2048];
	char localFile[2048];
	int i;
	for(i=0;i<2048;i++)localFile[i] = 0;
	while(token != NULL){
		if(strstr(token, "PUT") != NULL || strstr(token, "GET") != NULL){
			strcpy(startLine, token);
			break;
		}
		token = strtok(NULL, "\n");
	}
	
	char* token2 = strtok(startLine, " ");
	int fileNext = 0;
	while(token2 != NULL){
		if(fileNext == 1){
			strcpy(localFile, token2);
			break;
		}
		else if(strstr(token2, "PUT") != NULL || strstr(token2, "GET") != NULL){fileNext = 1;}
		token2 = strtok(NULL, " ");
	}
	strcpy(fileName, localFile);
}
void setParameters(void) {
	/*function to parse ws.conf*/
	char filename[2048];
	strcpy(filename, docroot);
	strcat(filename, "dfs.conf");
	printf("Filename: %s\n", filename);
	FILE* conf = fopen(filename, "r");
	if(conf == NULL){
		printf("No dfs.conf detected in document root! Please provide one!\n");
		exit(1);
	}
	char line[2048];
	int compatp = 0;
	while(fgets(line, sizeof line, conf) != NULL){
		if(strstr(line, "#") == NULL && strlen(line) > 1){
			char* token = strtok(line, " ");
			strcpy(userpass[numUsers], "");
			while(token != NULL){
				strcat(userpass[numUsers], token);
				strcat(userpass[numUsers], ":");
				token = strtok(NULL, " ");
			}
			int newlen = strlen(userpass[numUsers]);
			int z;
			for(z=newlen-2;z<newlen;z++)userpass[numUsers][z] = '\0';
			numUsers++;
		}
		int x;
		for(x=0;x<2048;x++)line[x] = 0;
	}
	fclose(conf);
}
int userIsValid(char* packeto, char* username){
	char packet[8192];
	char name[2048];
	char pass[2048];
	int local;
	strcpy(packet, packeto);
	strcpy(username, "");
	char* token = strtok(packet, "\n");
	while(token != NULL){
		if(strstr(token, "NAME") != NULL){
			char* subtoken = strtok(token, " ");
			int step = 0;
			while(subtoken != NULL){
				if(step == 1)strcpy(name, subtoken);
				subtoken = strtok(NULL, " ");
				step++;
			}	
		}
		token = strtok(NULL, "\n");
	}
	strcpy(packet, packeto);
	token = strtok(packet, "\n");
	while(token != NULL){
		if(strstr(token, "WORD") != NULL){
			char* subtoken = strtok(token, " ");
			int step = 0;
			while(subtoken != NULL){
				if(step == 1)strcpy(pass, subtoken);
				subtoken = strtok(NULL, " ");
				step++;
			}	
		}
		token = strtok(NULL, "\n");
	}
	if(strlen(name) < 1)return 0;
	int i;
	for(i=0;i < numUsers;i++){
		if(strstr(userpass[i], name) != NULL){
			char linecop[2048];
			strcpy(linecop, userpass[i]);
			char* subtoken = strtok(linecop, ":");
			int step = 0;
			while(subtoken != NULL){
				if(step == 1){
					if(!strcmp(pass, subtoken)){
						strcpy(username, name);
						return 1;
					}	
				}
				subtoken = strtok(NULL, ":");
				step++;
			}
		}
	}
	return 0;
}

int getRequestType(char* packeto){
	char packet[8192];
	strcpy(packet, packeto);
	char* token = strtok(packet, "\n");
	while(token != NULL){
		if(strstr(token, "LIST") != NULL)return 2;
		else if(strstr(token, "GET") != NULL)return 0;
		else if(strstr(token, "PUT") != NULL)return 1;
		token = strtok(NULL, "\n");
	}
	return -1;
}

void sendList(int sock, char* directory, char* uname){
	DIR* dp;
	struct dirent* ep;
	char dircontents[8192];
	strcpy(dircontents, "");
	char* sendcontents = "LIST\nUSERNAME: %s\n%s\n";
	char localdir[2048];
	char str[8192];
	strcpy(localdir, "./");
	strcat(localdir, directory);
	dp = opendir(localdir);
	if(dp != NULL){
		while(ep = readdir(dp)){
			if(!!strcmp(ep -> d_name, ".") && !!strcmp(ep -> d_name, "..")){
				strcat(dircontents, ep -> d_name);
				strcat(dircontents, "\n");
			}
		}
	}
	sprintf(str, sendcontents, uname, dircontents);
	int length = strlen(str);
	unsigned char* msg = str;
	do{
		int numsent = send(sock, msg, length, MSG_NOSIGNAL);
		if(numsent < 0)break;
		msg += numsent;
		length -= numsent;
	}while(length > 0);
	closedir(dp);
}
void sendError(int sock, char* info){
	char* sendcontents = "ERROR\n%s\n";
	char str[1024];
	sprintf(str, sendcontents, info);
	int length = strlen(str);
	unsigned char* msg = str;
	do{
		int numsent = send(sock, msg, length, MSG_NOSIGNAL);
		if(numsent < 0)break;
		msg += numsent;
		length -= numsent;
	}while(length > 0);
}
void receiveFile(int sock, char* workingdirectory, char* filename){
	char str[4096];
	char* msg = "ACK OK %s\n\n";
	sprintf(str, msg, filename);
	
	int length = strlen(str);
	unsigned char* bufptr = str;
	do{
		int numsent = send(sock, bufptr, length, MSG_NOSIGNAL);
		if(numsent < 0){
			printf("Bad: %d\n", numsent);
			break;
		}
		bufptr += numsent;
		length -= numsent;
	}while(length > 0);

	char fullFileName[4096];
	strcpy(fullFileName, workingdirectory);
	strcat(fullFileName, "/");
	strcat(fullFileName, filename);
	FILE* file = fopen(fullFileName, "w");
	char buf[4096];
	int i;
	int copied;
	
	copied = recv(sock, buf, sizeof(buf), 0);
	fwrite(buf, 1, copied, file);
	for(i=0;i<4096;i++)buf[i]=0;
	while(copied = recv(sock, buf, sizeof(buf), 0)){
		if(copied == 0)break;
		fwrite(buf, 1, copied, file);
		int i;
		for(i=0;i<4096;i++)buf[i]=0;
	}
	fclose(file);
}
void sendFile(int sock, char* workingdirectory, char* filename){
	char str[4096];
        char* msg = "ACK OK %s\n\n";
        sprintf(str, msg, filename);

        int length = strlen(str);
        unsigned char* bufptr = str;
        do{
                int numsent = send(sock, bufptr, length, MSG_NOSIGNAL);
                if(numsent < 0){
                        printf("Bad: %d\n", numsent);
                        break;
                }
                bufptr += numsent;
                length -= numsent;
        }while(length > 0);

        char fullFileName[4096];
        strcpy(fullFileName, workingdirectory);
        strcat(fullFileName, "/");
        strcat(fullFileName, filename);

        FILE* file = fopen(fullFileName, "r");
        char buf[4096];
        int i;
        int copied;

        copied = recv(sock, buf, sizeof(buf), 0);
	unsigned char readbuf[1024];
        while(!feof(file)){
                int numread = fread(readbuf, sizeof(unsigned char), 1024, file);
                if(numread < 0)break;
                unsigned char* bufptr = readbuf;
                do{
                        int numsent = send(sock, bufptr, numread, MSG_NOSIGNAL);
                        if(numsent < 0){
                                break;
                        }
                        bufptr += numsent;
                        numread -= numsent;
                }while(numread > 0);
                        
        }
	shutdown(sock, SHUT_WR);
	recv(sock, NULL, 0, 0);
        fclose(file);
}
