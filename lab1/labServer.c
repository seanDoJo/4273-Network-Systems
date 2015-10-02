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
int hostsock(const char* portnum, int waitLen);
int isDefaultFile(char* fileName);
void handleRequest(void* ptr);
void setParameters(char* filename);
int getFileName(char* packet, char fileName[]);
int methodOk(char* packeto, char* returned);
int httpOk(char* packeto, char* returned);
int uriOk(char* uri);
void sendServerError(int sock);
void sendFile(int sock, char* fileName, int keepalive);
void sendBad(int sock, char* description, int type);
void sendNotFound(int sock, char* fileName);
void getContentType(char* fileName, char* content);
void getKeepAlive(char* packeto, int* keepalive);
struct targs {
	int newSock;
};
char portnum[32];
char docroot[1024];
char ind[10][1024];
char compat[32][1024];

int main(int argc, char* argv[]){
	
	if(argc < 2){
		printf("need to provide a conf file!\n");
		exit(1);
	}
	setParameters(argv[1]);
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
			sendServerError(incoming);
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
	char packets[10][4096];
	char packet[8192];
	int packetIndex;

	strcpy(packet, "");
	sock = args -> newSock;

	//read in data
	recv(sock, buf, sizeof(buf), 0);
	int x;
	strcat(packet, buf);
	for(x=0;x<4096;x++)buf[x] = 0;
	int keepalive = 1;
	do{
		packetIndex = -1;
		//read whatever is on the socket fully
		while(copied = recv(sock, buf, sizeof(buf), MSG_DONTWAIT) > 0){
			int i;
			strcat(packet, buf);
			for(i=0;i<4096;i++)buf[i] = 0;
		}
		//extract data from packet
		if(strlen(packet) > 0){
			char packetsCop[8192];
			strcpy(packetsCop, packet);

			//split the received data into packets
			char* token = strtok(packetsCop, "\n");
			while(token != NULL){
				int e = strlen(token);
				if(strstr(token, "HTTP/") != NULL){
					packetIndex += 1;
					strcpy( packets[packetIndex], "");
				}
				strcat(packets[packetIndex], token);
				strcat(packets[packetIndex], "\n");
				token = strtok(NULL, "\n");
			}
			int t;

			//iterate throught each packet and process appropriately
			for(t = 0; t <= packetIndex; t++){
				//update our timeout
				seconds = time(NULL);
				char method[2048];
				char http[512];

				//check if the method is ok
				if(methodOk(packets[t], method) < 0){
					sendBad(sock, method, 0); //send 400: invalid method
					keepalive = 0;
				}
				//check if the http version is ok
				else if(httpOk(packets[t], http) < 0){
					sendBad(sock, http, 2); //send 400: invalid http version
					keepalive = 0;
				}
				else{
					//check whether or not we want to keep the connection open
					getKeepAlive(packets[t], (int*)&keepalive);
					char fileName[4096];
					int status;
					
					//get the file name from the packet and do additional processing
					status = getFileName(packets[t], fileName);

					//determine what to do with packet
					if(status == 200){
						sendFile(sock, fileName, keepalive);
					}
					else if(status == 404){
						sendNotFound(sock, fileName);
						keepalive = 0;
					}
					else if(status == 400){
						sendBad(sock, fileName, 1);
						keepalive = 0;
					}
					else if(status == 501){
						sendBad(sock, fileName, 3);
						keepalive = 0;
					}
				}
			}
			int r;
			for(r=0;r<packetIndex;r++)packets[r][0] = 0;
		}
		int i;
		for(i=0;i<sizeof(packet);i++)packet[i] = 0;
		strcpy(packet, "");
	}while(keepalive && ((time(NULL)) - (seconds) < 10));
	close(sock);	
	free(args);
	pthread_exit((void*)(0));
}

void getKeepAlive(char* packeto, int* keepalive){
	char packet[8192];
	strcpy(packet, packeto);
	char* token = strtok(packet, "\n");
	char keepline[2048];
	int i;
	for(i=0;i<2048;i++)keepline[i] = 0;
	while(token != NULL){
		if(strstr(token, "connection") != NULL){
			strcpy(keepline, token);
			break;
		}
		token = strtok(NULL, "\n");
	}
	char* token2 = strtok(keepline, " ");
	int keepaliveNext = 0;
	while(token2 != NULL){
		if(keepaliveNext == 1){
			if(strstr(token2, "live") != NULL) *keepalive = 1;
			else *keepalive = 0;
			break;
		}
		else if(strstr(token2, "Connection") != NULL){keepaliveNext = 1;}
		token2 = strtok(NULL, " ");
	}
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
		if(strstr(token, "GET") != NULL){
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
		else if(strstr(token2, "GET") != NULL){fileNext = 1;}
		token2 = strtok(NULL, " ");
	}
	//check that the uri is valid
	if(uriOk(localFile) < 0){
		strcpy(fileName, localFile);
		return 400;
	}
	char content[1024];
	getContentType(localFile, content);
	//check if the file is one of the default index files, and change to index.html 
	if(isDefaultFile(localFile)){
		strcpy(fileName, "");
		strcat(fileName, docroot);
		strcat(fileName, "/index.html");
		return 200;
	}
	//check if content type of file is supported
	else if(!strcmp(content, "NONE")){
		strcpy(fileName, localFile);
		return 501;
	}
	else{
		char fullpath[4096];
		strcpy(fullpath, docroot);
		strcat(fullpath, localFile);
		//check if file exists
		if(access(fullpath, F_OK) != -1){
			strcpy(fileName, fullpath);
			return 200;
		}
		else{
			strcpy(fileName, localFile);
			return 404;
		}
	}
}
void setParameters(char* filename) {
	/*function to parse ws.conf*/
	FILE* conf = fopen(filename, "r");
	char line[2048];
	char linecpy[2048];
	int compatp = 0;
	while(fgets(line, sizeof line, conf) != NULL){
		int len;
		for(len=0;line[len] != '\n' && len < sizeof line; len++);
		strcpy(linecpy, line);
		char* token = strtok(line, " ");
		while(token != NULL){
			if(strstr(token, "Listen")){
				memcpy(portnum, &linecpy[7], len - 7);
				break;
			}
			else if(strstr(token, "DocumentRoot")){
				memcpy(docroot, &linecpy[14], len - 15);
				break;
			}
			else if(strstr(token, "DirectoryIndex")){
				char subline[1024];
				memcpy(subline, &linecpy[15], len - 15);
				char* subtoken = strtok(subline, " ");
				int place = 0;
				while(subtoken != NULL){
					strcpy(ind[place], subtoken);
					subtoken = strtok(NULL, " ");
					place++;
				}
			}
			else if(strstr(token, ".")){
				strcpy(compat[compatp], linecpy);
				compatp++;
				break;
			}
			token = strtok(NULL, " ");
		}
	}
	fclose(conf);
}
void sendBad(int sock, char* description, int type){
	/*function to send 400 error*/
	char str[4096];
	char bod[4096];
	char* poss[4] = {
			"<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request: Invalid Method: %s</h1></body></html>",
			"<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request: Invalid URI: %s</h1></body></html>",
			"<html><head><title>400 Bad Request</title></head><body><h1>400 Bad Request: Invalid HTTP-Version: %s</h1></body></html>",
			"<html><head><title>501 Not Implemented</title></head><body><h1>501 Not Implemented: %s</h1></body></html>"
	};
	sprintf(bod, poss[type], description);
	char* msg;
	if(type < 3){
		msg = "HTTP/1.1 400 Bad Request\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %d\r\n\r\n%s";
	}
	else {
		msg = "HTTP/1.1 501 Bad Request\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %d\r\n\r\n%s";
	}
	int sendsize = strlen(bod);
	sprintf(str, msg, sendsize, bod);
	printf("%s\n", str);
	int length = strlen(str);
	do{
		int numsent = send(sock, str, length, MSG_NOSIGNAL);
		if(numsent < 0)break;
		msg += numsent;
		length -= numsent;
	}while(length > 0);
}
void sendServerError(int sock){
	/*send 500 error*/
	char str[4096];
	char* error = "<html><head><title>500 Server Error</title></head><body><h1>500 Internal Server Error</h1></body></html>";
	char* msg = "HTTP/1.1 500 Internal\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %d\r\n\r\n%s";	
	int sendsize = strlen(error);
	sprintf(str, msg, sendsize, error);
	int length = strlen(str);
	do{
		int numsent = send(sock, str, length, MSG_NOSIGNAL);
		if(numsent < 0)break;
		msg += numsent;
		length -= numsent;
	}while(length > 0);
}
void sendNotFound(int sock, char* fileName){
	/*send 404 error*/
	char str[4096];
	char bod[4096];
	char* notfound = "<html><head><title>404 NOT FOUND</title></head><body><h1>404 Not Found: %s</h1></body></html>";
	sprintf(bod, notfound, fileName);
	char* msg = "HTTP/1.1 404 Not Found\r\n"
			"Connection: close\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %d\r\n\r\n%s";
	int sendsize = strlen(bod);
	sprintf(str, msg, sendsize, bod);
	printf("%s\n", str);
	int length = strlen(str);
	do{
		int numsent = send(sock, str, length, MSG_NOSIGNAL);
		if(numsent < 0)break;
		msg += numsent;
		length -= numsent;
	}while(length > 0);
}
	
void sendFile(int sock, char* fileName, int keepalive){
	struct stat st;
	stat(fileName, &st);
	int doclength = st.st_size;
	FILE* page = fopen(fileName, "r");

	//Construct header with correct data
	char str[4096];
	char content[1024];
	char keep[1024];
	getContentType(fileName, content);
	if(!strcmp(content, "NONE")){
		sendBad(sock,fileName,3);
		return;
	}
	char* token = strtok(content, " ");		
	while(token != NULL){
		if(strstr(token, "/") != NULL){
			strcpy(content, token);
			break;
		}	
		token = strtok(NULL, " ");
	}
	if(keepalive)strcpy(keep, "keep-alive");
	else strcpy(keep, "close");
	char* msg = "HTTP/1.1 200 OK\r\n"
			"Content-Type: %s"
			"Content-Length: %d\r\n"
			"Connection: %s\r\n\r\n";
	sprintf(str, msg, content, doclength, keep);
	printf("%s\n",str);
	
	//Send header data
	int length = strlen(str);
	do{
		int numsent = send(sock, str, length, MSG_NOSIGNAL);
		if(numsent < 0)break;
		msg += numsent;
		length -= numsent;
	}while(length > 0);

	//send file contents as binary
	unsigned char readbuf[1024];
	while(!feof(page)){
		int numread = fread(readbuf, sizeof(unsigned char), 1024, page);
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
	fclose(page);
}
int isDefaultFile(char* file) {
	if(!strcmp(file, "/"))return 1;
	char filep[4096];
	int i, x;
	for(i = 0; i < 10; i++){
		strcpy(filep, "/");
		strcat(filep, ind[i]);
		if(!strcmp(file, ind[i]) || !strcmp(file, filep)) return 1;
		for(x = 0; x < 4096; x++)filep[x] = 0;
	}
	return 0;
}

void getContentType(char* fileName, char* content){
	int i;
	for(i = 0; i < 32; i++){
		if(strlen(compat[i]) > 1){
			char cp[2048];
			strcpy(cp, compat[i]);
			char* token = strtok(cp, " ");		
			while(token != NULL){
				if(strstr(token, ".")){
					if(strstr(fileName, token) != NULL){
						strcpy(content, compat[i]);
						return;
					}
					break;
				}
				token = strtok(NULL, " ");
			}
		}
	}
	strcpy(content, "NONE");
}
int methodOk(char* packeto, char* returned){
	char packet[8192];
	strcpy(packet, packeto);
	char* token = strtok(packet, "\n");
	char* line = strtok(token, " ");
	strcpy(returned, line);
	if(strstr(line, "GET") != NULL) return 0;
	else return -1;
}
int httpOk(char* packeto, char* returned){
	char packet[8192];
	strcpy(packet, packeto);
	char* token = strtok(packet, "\n");
	char startline[2048];
	while(token != NULL){
		if(strstr(token, "HTTP") != NULL){
			strcpy(startline, token);
			break;
		}
		token = strtok(NULL, "\n");
	}
	char* line = strtok(startline, " ");
	while(line != NULL){
		if(strstr(line, "HTTP") != NULL){
			strcpy(returned, line);
			if(strstr(line, "1.0") != NULL || strstr(line, "1.1") != NULL) return 0;
			else return -1;
		}
		line = strtok(NULL, " ");
	}
}
int uriOk(char* uri){
	regex_t regex;
	int reti;
	reti = regcomp(&regex, "^[[:alnum:]/]", 0);
	reti = regexec(&regex, uri, 0, NULL, 0);
	if(!reti){
		return 0;
	}
	else if(reti == REG_NOMATCH){
		return -1;
	}
	else{
		printf("ERROR\n");
		return -1;
	}
	regfree(&regex);
}
