#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <openssl/md5.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
void setParameters(char* conf);
void getHostInfo(int server, char* addr, char* portnum);
void list(void);
void push(void);
void get(void);
void md5sum(FILE* file, int* p1, int* p2, int* p3, int* p4);
void setSock(int* sock, int index, char* name);
char servers[4][512];
int numServers = 0;
char uname[512];
char pass[1024];
int serverConf[4][4][2];
int main(int argc, char* argv[]){
	if(argc < 2){
		printf("NEED TO SPECIFY A DFC.CONF\n");
		exit(1);
	}
	strcpy(uname, "");
	strcpy(pass, "");
	setParameters(argv[1]);
	int i;
	printf("Distributed File System\n\nFile Server Pool:\n\n");
	for(i=0;i<numServers;i++)printf("Server: %s\n", servers[i]);
	while(1){
		int option;
		printf("\n1. List available files\n2. Push a local file to the server\n3. Retrieve a file from the server\n\n> ");
		if(scanf("%d", &option) >= 0){
			if(option == 1)list();
			else if(option == 2)push();
			else if(option == 3)get();
		}
		while(getchar() != '\n');
	}
}
void setParameters(char* conf){
	FILE* fd = fopen(conf, "r");
	if(fd == NULL){
		printf("provided file doesn't exist!\n");
		exit(1);
	}
	serverConf[0][0][0] = 1;
	serverConf[0][0][1] = 2;
	serverConf[0][1][0] = 2;
	serverConf[0][1][1] = 3;
	serverConf[0][2][0] = 3;
	serverConf[0][2][1] = 4;
	serverConf[0][3][0] = 4;
	serverConf[0][3][1] = 1;

	serverConf[1][0][0] = 4;
	serverConf[1][0][1] = 1;
	serverConf[1][1][0] = 1;
	serverConf[1][1][1] = 2;
	serverConf[1][2][0] = 2;
	serverConf[1][2][1] = 3;
	serverConf[1][3][0] = 3;
	serverConf[1][3][1] = 4;

	serverConf[2][0][0] = 3;
	serverConf[2][0][1] = 4;
	serverConf[2][1][0] = 4;
	serverConf[2][1][1] = 1;
	serverConf[2][2][0] = 1;
	serverConf[2][2][1] = 2;
	serverConf[2][3][0] = 2;
	serverConf[2][3][1] = 3;

	serverConf[3][0][0] = 2;
	serverConf[3][0][1] = 3;
	serverConf[3][1][0] = 3;
	serverConf[3][1][1] = 4;
	serverConf[3][2][0] = 4;
	serverConf[3][2][1] = 1;
	serverConf[3][3][0] = 1;
	serverConf[3][3][1] = 2;


	char line[2048];
	while(fgets(line, sizeof line, fd) != NULL){
		if(strstr(line, "Server") != NULL){
			strcpy(servers[numServers], "");
			char* token = strtok(line, " ");
			int linestep = 0;
			while(token != NULL){
				if(linestep == 0 && !strcmp(token, "Server"))linestep++;
				else if(linestep == 1){
					strcat(servers[numServers], token);
					strcat(servers[numServers], ":");
					linestep++;
				}
				else if(linestep == 2){
					strcat(servers[numServers], token);
					linestep++;
				}
				token = strtok(NULL, " ");
			}
			int len = strlen(servers[numServers]);
			servers[numServers][len-1] = 0;
			numServers++;
		}
		else if(strstr(line, "Username") != NULL){
			char* token = strtok(line, " ");
			int linestep = 0;
			while(token != NULL){
				if(linestep == 1){
					strcpy(uname, token);
					int len = strlen(uname);
					uname[len-1] = 0;
				}
				else linestep++;
				token = strtok(NULL, " ");
			}
		}
		else if(strstr(line, "Password") != NULL){
			char* token = strtok(line, " ");
			int linestep = 0;
			while(token != NULL){
				if(linestep == 1){
					strcpy(pass, token);
					int len = strlen(pass);
					pass[len-1] = 0;
				}
				else linestep++;
				token = strtok(NULL, " ");
			}
		}	
	}
	fclose(fd);
}
void list(void){
	struct sockaddr_in sin;
	int sock;
	int i;
	char files[30][1024];
	int numFiles = 0;
	int error = 0;
	for(i=0; i<numServers;i++){
		char addr[2048];
		char port[2048];
		getHostInfo(i, addr, port);	
		memset(&sin, 0, sizeof(sin));
		sin.sin_family = AF_INET;
		sin.sin_port = htons((unsigned short)atoi(port));
		sin.sin_addr.s_addr = inet_addr(addr);
		sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(connect(sock, (struct sockaddr*)&sin, sizeof(sin)) > -1){
			char* rawstr = "LIST\nUSERNAME: %s\nPASSWORD: %s\n\n";
			char testbuf[1024];
			sprintf(testbuf, rawstr, uname, pass);
			int len = strlen(testbuf);
			send(sock, testbuf, len, MSG_NOSIGNAL);
			char buf[4096];
			char packet[8192];
			strcpy(packet, "");
			int copied;
			recv(sock, buf, sizeof(buf), 0);
			strcat(packet, buf);
			int i;
			for(i=0;i<4096;i++)buf[i]=0;
			time_t seconds = time(NULL);
			while(copied = recv(sock, buf, sizeof(buf), MSG_PEEK) > 0 && (time(NULL) - seconds) < 1){
				if(strlen(buf) > 0){
					strcat(packet, buf);
					char trashbuf[4096];
					recv(sock, trashbuf, sizeof(trashbuf), 0);
					seconds = time(NULL);
				}
				int i;
				for(i=0;i<4096;i++)buf[i]=0;
			}
			char packetcop[2048];
			strcpy(packetcop, packet);
			char* token = strtok(packet, "\n");
			int linestep = 0;
			while(token != NULL){
				if(linestep == 0){
					if(!!strcmp(token, "LIST")){
						printf("%s\n", packetcop);
						error = 1;
					}
				}
				else if(linestep > 1 && !error){
					int x,placefound = 0;
					char raw[2048];
					strcpy(raw, token);
					int j;
					for(j=0;j<strlen(raw);j++)raw[j] = raw[j+1]; 
					raw[strlen(raw)-2]='\0';
					char piece[2];
					piece[0] = token[strlen(token)-1];
					piece[1] = '\0';
					for(x=0;x<numFiles;x++){
						if(strstr(files[x], raw) != NULL){
							strcat(files[x], ":");
							strcat(files[x], piece);
							placefound = 1;
							break;
						}
					}
					if(placefound == 0){
						char file[2048];
						strcpy(file, token);
						int j;
						for(j=0;j<strlen(file);j++)file[j] = file[j+1]; 
						file[strlen(file)-2]='\0';
						strcpy(files[numFiles], file);
						strcat(files[numFiles],":");
						strcat(files[numFiles],piece);
						numFiles++;
					}					
				}
				else if (linestep > 0 && error){
					printf("%s\n",token);
				}
				linestep++;
				token = strtok(NULL, "\n");
			}
		}
	}
	if(!error){
		printf("\nAvailable Files:\n\n");
		int z;
		for(z=0;z<numFiles;z++){
			char fname[512];
			char fline[1024];
			strcpy(fline, files[z]);
			char* token = strtok(fline, ":");
			int linestep = 0;
			char partsOk[4];
			int p,ok=1;
			for(p=0;p<4;p++)partsOk[p] = 0;
			while(token != NULL){
				if(linestep == 0){
					strcpy(fname, token);
				}
				else{
					int piece = token[0] - '0' - 1;
					partsOk[piece] = 1;
				}
				token = strtok(NULL, ":");
				linestep++;
			}
			for(p=0;p<4;p++){
				if(partsOk[p] == 0){
					ok = 0;
					break;
				}
			}
			if(!ok) printf("\t%s [incomplete]\n", fname);
			else printf("\t%s\n",fname);
		}
	}
}
void push(void){
	char fname[2048];
	char fnamep1[2048];
	char fnamep2[2048];
	char fnamep3[2048];
	char fnamep4[2048];
	char readBuffer[4096];
	FILE* file;
	struct stat st;
	int p1, p2, p3, p4, hashIndex, sock1, sock2, sock1set, sock2set, index;
	int totalRead = 0;

	printf("\nFilename:\n\n> ");
	scanf("%s", fname);

	if(access(fname, F_OK) == -1){
		printf("Invalid Filename!\n");
		return;
	}
	
	strcpy(fnamep1, ".");
	strcpy(fnamep2, ".");
	strcpy(fnamep3, ".");
	strcpy(fnamep4, ".");

	strcat(fnamep1, fname);
	strcat(fnamep2, fname);
	strcat(fnamep3, fname);
	strcat(fnamep4, fname);

	strcat(fnamep1, ".1");	
	strcat(fnamep2, ".2");
	strcat(fnamep3, ".3");
	strcat(fnamep4, ".4");

	stat(fname, &st);
	int size = st.st_size;

	file = fopen(fname, "r");
	md5sum(file, &p1, &p2, &p3, &p4);
	fclose(file);

	file = fopen(fname, "r");
	hashIndex = (p1 ^ p2 ^ p3 ^ p4) % 4;

	//send first block
	sock1set = 0;
	sock2set = 0;
	for(index=0;index<4;index++){
		if(serverConf[hashIndex][index][0] == 1 || serverConf[hashIndex][index][1] == 1){
			if(sock1set < 1){
				setSock(&sock1, index, fnamep1);
				sock1set = 1;
			} else if(sock2set < 1){
				setSock(&sock2, index, fnamep1);
				sock2set = 1;
			}
		}
	}
	if(&sock1 == NULL || &sock2 == NULL){
		printf("Invalid user credentials!\n");
		fclose(file);
		return;
	}
	while(totalRead < size/4){
		unsigned char readbuf[1];
		int numread;
        	int staticread = fread(readbuf, sizeof(unsigned char), 1, file);
		unsigned char* bufptr;
		totalRead += staticread;

		//send to first server
		numread = staticread;
                //if(numread < 0)break;
                bufptr = readbuf;
		do{
			int numsent = send(sock1, bufptr, numread, MSG_NOSIGNAL);
			if(numsent < 0){
				break;
			}
			bufptr += numsent;
			numread -= numsent;
		}while(numread > 0);
		
		//send to second server
		numread = staticread;
                bufptr = readbuf;
                do{
                        int numsent = send(sock2, bufptr, numread, MSG_NOSIGNAL);
                        if(numsent < 0){
                                break;
                        }
                        bufptr += numsent;
                        numread -= numsent;
                }while(numread > 0);
	}
	shutdown(sock1, SHUT_WR);
	shutdown(sock2, SHUT_WR);
	recv(sock1, NULL, 0, 0);
	recv(sock2, NULL, 0, 0);
	close(sock1);
	close(sock2);		
	//send second block
	sock1set = 0;
	sock2set = 0;
	for(index=0;index<4;index++){
		if(serverConf[hashIndex][index][0] == 2 || serverConf[hashIndex][index][1] == 2){
			if(sock1set < 1){
				setSock(&sock1, index, fnamep2);
				sock1set = 1;
			} else if(sock2set < 1){
				setSock(&sock2, index, fnamep2);
				sock2set = 1;
			}
		}
	}
	if(&sock1 == NULL || &sock2 == NULL){
		printf("Invalid user credentials!\n");
		fclose(file);
		return;
	}
	while(totalRead < size/2){
		unsigned char readbuf[1];
		int numread;
        	int staticread = fread(readbuf, sizeof(unsigned char), 1, file);
		unsigned char* bufptr;
		totalRead += staticread;

		//send to first server
		numread = staticread;
                bufptr = readbuf;
                do{
                        int numsent = send(sock1, bufptr, numread, MSG_NOSIGNAL);
                        if(numsent < 0){
                                break;
                        }
                        bufptr += numsent;
                        numread -= numsent;
                }while(numread > 0);
		
		//send to second server
		numread = staticread;
                bufptr = readbuf;
                do{
                        int numsent = send(sock2, bufptr, numread, MSG_NOSIGNAL);
                        if(numsent < 0){
                                break;
                        }
                        bufptr += numsent;
                        numread -= numsent;
                }while(numread > 0);
	}
	shutdown(sock1, SHUT_WR);
	shutdown(sock2, SHUT_WR);
	recv(sock1, NULL, 0, 0);
	recv(sock2, NULL, 0, 0);
	close(sock1);
	close(sock2);		

	//send third block
	sock1set = 0;
	sock2set = 0;
	for(index=0;index<4;index++){
		if(serverConf[hashIndex][index][0] == 3 || serverConf[hashIndex][index][1] == 3){
			if(sock1set < 1){
				setSock(&sock1, index, fnamep3);
				sock1set = 1;
			} else if(sock2set < 1){
				setSock(&sock2, index, fnamep3);
				sock2set = 1;
			}
		}
	}
	if(&sock1 == NULL || &sock2 == NULL){
		printf("Invalid user credentials!\n");
		fclose(file);
		return;
	}
	while(totalRead < (size*3)/4){
		unsigned char readbuf[1];
		int numread;
        	int staticread = fread(readbuf, sizeof(unsigned char), 1, file);
		unsigned char* bufptr;
		totalRead += staticread;

		//send to first server
		numread = staticread;
                if(numread < 0)break;
                bufptr = readbuf;
                do{
                        int numsent = send(sock1, bufptr, numread, MSG_NOSIGNAL);
                        if(numsent < 0){
                                break;
                        }
                        bufptr += numsent;
                        numread -= numsent;
                }while(numread > 0);
		
		//send to second server
		numread = staticread;
                if(numread < 0)break;
                bufptr = readbuf;
                do{
                        int numsent = send(sock2, bufptr, numread, MSG_NOSIGNAL);
                        if(numsent < 0){
                                break;
                        }
                        bufptr += numsent;
                        numread -= numsent;
                }while(numread > 0);
	}
	shutdown(sock1, SHUT_WR);
	shutdown(sock2, SHUT_WR);
	recv(sock1, NULL, 0, 0);
	recv(sock2, NULL, 0, 0);
	close(sock1);
	close(sock2);		

	//send fourth block
	sock1set = 0;
	sock2set = 0;
	for(index=0;index<4;index++){
		if(serverConf[hashIndex][index][0] == 4 || serverConf[hashIndex][index][1] == 4){
			if(sock1set < 1){
				setSock(&sock1, index, fnamep4);
				sock1set = 1;
			} else if(sock2set < 1){
				setSock(&sock2, index, fnamep4);
				sock2set = 1;
			}
		}
	}
	if(&sock1 == NULL || &sock2 == NULL){
		printf("Invalid user credentials!\n");
		fclose(file);
		return;
	}
	while(totalRead < size){
		unsigned char readbuf[1];
		int numread;
        	int staticread = fread(readbuf, sizeof(unsigned char), 1, file);
		unsigned char* bufptr;
		totalRead += staticread;

		//send to first server
		numread = staticread;
                if(numread < 0)break;
                bufptr = readbuf;
                do{
                        int numsent = send(sock1, bufptr, numread, MSG_NOSIGNAL);
                        if(numsent < 0){
                                break;
                        }
                        bufptr += numsent;
                        numread -= numsent;
                }while(numread > 0);
		
		//send to second server
		numread = staticread;
                if(numread < 0)break;
                bufptr = readbuf;
                do{
                        int numsent = send(sock2, bufptr, numread, MSG_NOSIGNAL);
                        if(numsent < 0){
                                break;
                        }
                        bufptr += numsent;
                        numread -= numsent;
                }while(numread > 0);
	}
	shutdown(sock1, SHUT_WR);
	shutdown(sock2, SHUT_WR);
	recv(sock1, NULL, 0, 0);
	recv(sock2, NULL, 0, 0);
	close(sock1);
	close(sock2);		

	fclose(file);
}
void setSock(int* sock, int index, char* name){
	struct sockaddr_in sin;
	char addr[2048];
	char port[2048];
	char str[4096];
	char buf[4096];
	int x;

	getHostInfo(index, addr, port);
	memset(&sin, 0, sizeof(sin));

	sin.sin_family = AF_INET;
	sin.sin_port = htons((unsigned short)atoi(port));
	sin.sin_addr.s_addr = inet_addr(addr);

	(*sock) = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	int ret = connect((*sock), (struct sockaddr*)&sin, sizeof(sin));
	if(ret > -1){
		char* msg = "PUT %s\nUSERNAME: %s\nPASSWORD: %s\n\n";
		sprintf(str, msg, name, uname, pass);

		int length = strlen(str);
		do{
			int numsent = send((*sock), str, length, MSG_NOSIGNAL);
			if(numsent < 0)break;
			msg += numsent;
			length -= numsent;
		}while(length > 0);
		time_t seconds = time(NULL);
		while(1){
			int copied = recv((*sock), buf, sizeof(buf), MSG_DONTWAIT);
			if(copied > 0){
				if(strstr(buf, "ACK OK") == NULL){
					printf("%s\n", buf);
					sock = NULL;
				}
				for(x=0;x<4096;x++)buf[x] = 0;
				break;
			} else if(time(NULL) - seconds >= 1){
				sock = NULL;
				break;
			}
		}
	}else{
		printf("Couldn't connect to server %s on port %s!\n", addr, port);
	}
}

void getHostInfo(int server, char* addr, char* portnum){
	char line[2048];
	strcpy(line, servers[server]);
	char* token = strtok(line, ":");
	int linestep = 0;
	while(token != NULL){
		if(linestep == 1){
			strcpy(addr, token);
		}
		else if(linestep == 2){
			strcpy(portnum, token);
		}
		token = strtok(NULL, ":");
		linestep++;
	}
}
void md5sum(FILE* file, int* p1, int* p2, int* p3, int* p4){
    char c[MD5_DIGEST_LENGTH];
    int i;
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, file)) != 0)
        MD5_Update (&mdContext, data, bytes);
    MD5_Final (c,&mdContext);
    sscanf( &c[0], "%x", p1 );
    sscanf( &c[8], "%x", p2 );
    sscanf( &c[16], "%x", p3 );
    sscanf( &c[24], "%x", p4 );
}
void get(void){
	struct sockaddr_in sin;
        int sock;
	int parts[4][2];
	char partNames[4][2][256];
	char fname[256];
        int i;
	for(i=0;i<4;i++){
		parts[i][0] = -1;
		parts[i][1] = -1;
		strcpy(partNames[i][0], "");
		strcpy(partNames[i][1], "");
	}
        char files[30][1024];
        int numFiles = 0;
        int error = 0;	

	printf("\nFilename:\n\n> ");
	scanf("%s", fname);


	for(i=0; i<numServers;i++){
                char addr[2048];
                char port[2048];
                getHostInfo(i, addr, port);
                memset(&sin, 0, sizeof(sin));
                sin.sin_family = AF_INET;
                sin.sin_port = htons((unsigned short)atoi(port));
                sin.sin_addr.s_addr = inet_addr(addr);
                sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
                if(connect(sock, (struct sockaddr*)&sin, sizeof(sin)) > -1){
                        char* rawstr = "LIST\nUSERNAME: %s\nPASSWORD: %s\n\n";
                        char testbuf[1024];
                        sprintf(testbuf, rawstr, uname, pass);
                        int len = strlen(testbuf);
                        send(sock, testbuf, len, MSG_NOSIGNAL);
                        char buf[4096];
                        char packet[8192];
                        strcpy(packet, "");
                        int copied;
                        recv(sock, buf, sizeof(buf), 0);
                        strcat(packet, buf);
                        int l;
			int linestep = 0;
                        for(l=0;l<4096;l++)buf[l]=0;
                        while(copied = recv(sock, buf, sizeof(buf), MSG_PEEK) > 0){
                                if(strlen(buf) > 0){
                                        strcat(packet, buf);
                                        char trashbuf[4096];
                                        recv(sock, trashbuf, sizeof(trashbuf), 0);
                                }
                                int k;
                                for(k=0;k<4096;k++)buf[k]=0;
                        }
			//get parts out
			char packetcop[512];
			strcpy(packetcop, packet);
			char* token = strtok(packet, "\n");
			while(token != NULL){	
				if(linestep == 0){
					if(!!strcmp(token, "LIST")){
						printf("%s\n", packetcop);
						return;
					}
				}
				else if(linestep > 1){
						int x,placefound = 0;
						char raw[2048];
						strcpy(raw, token);
						int j;
						for(j=0;j<strlen(raw);j++)raw[j] = raw[j+1];
						raw[strlen(raw)-2]='\0';
						char piece[2];
						piece[0] = token[strlen(token)-1];
						piece[1] = '\0';
	
						if(!strcmp(raw, fname)){
							int part = atoi(piece) - 1;
							int index = 0;
							if(parts[part][0] >= 0)index=1;
							parts[part][index] = i;
							strcpy(partNames[part][index], token);
						}
							
				}
				linestep++;
				token = strtok(NULL, "\n");
			}
		}
		close(sock);
	}
	for(i=0;i<4;i++){
		if(parts[i][0] < 0 && parts[i][1] < 0){
			printf("\nCan't retrieve file!\nEither incomplete or doesn't exist!\n");
			return;
		}
	}
	char addr[2048];
	char port[2048];
	char str[2048];
	char buf[4096];
	char* msg = "GET %s\nUSERNAME: %s\nPASSWORD: %s\n\n";
	char* ack = "ACK READY\n\n";
	FILE* file = fopen(fname, "wb");
	int numread;
	unsigned char* bufptr;
	int copied;
	time_t seconds;
	//receive part 1
	sprintf(str, msg, partNames[0][0], uname, pass);

	getHostInfo(parts[0][0], addr, port);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((unsigned short)atoi(port));
	sin.sin_addr.s_addr = inet_addr(addr);
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	connect(sock, (struct sockaddr*)&sin, sizeof(sin));
	
	numread = strlen(str);
	bufptr = str;
	do{
		int numsent = send(sock, bufptr, numread, MSG_NOSIGNAL);
		if(numsent < 0){
			break;
		}
		bufptr += numsent;
		numread -= numsent;
	}while(numread > 0);
	
	recv(sock, buf, sizeof(buf), 0);
	for(i=0; i<4096; i++)buf[i]=0;
	
	numread = strlen(ack);
	bufptr = ack;
	do{
		int numsent = send(sock, bufptr, numread, MSG_NOSIGNAL);
		if(numsent < 0){
			break;
		}
		bufptr += numsent;
		numread -= numsent;
	}while(numread > 0);
	seconds = time(NULL);
	while(copied = recv(sock, buf, sizeof(buf), MSG_DONTWAIT)){
                if(copied == 0)break;
		if(copied > 0){
                	fwrite(buf, 1, copied, file);
                	int i;
                	for(i=0;i<4096;i++)buf[i]=0;
			seconds = time(NULL);
		} else if((time(NULL) - seconds) >= 1){
			break;
		}
	}
	close(sock);
	for(i=0;i<2048;i++){
		addr[i] = 0;
		port[i] = 0;
	}
	//receive part 2
	sprintf(str, msg, partNames[1][0], uname, pass);

	getHostInfo(parts[1][0], addr, port);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((unsigned short)atoi(port));
	sin.sin_addr.s_addr = inet_addr(addr);
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	connect(sock, (struct sockaddr*)&sin, sizeof(sin));
	
	numread = strlen(str);
	bufptr = str;
	do{
		int numsent = send(sock, bufptr, numread, MSG_NOSIGNAL);
		if(numsent < 0){
			break;
		}
		bufptr += numsent;
		numread -= numsent;
	}while(numread > 0);
	
	recv(sock, buf, sizeof(buf), 0);
	for(i=0; i<4096; i++)buf[i]=0;
	
	numread = strlen(ack);
	bufptr = ack;
	do{
		int numsent = send(sock, bufptr, numread, MSG_NOSIGNAL);
		if(numsent < 0){
			break;
		}
		bufptr += numsent;
		numread -= numsent;
	}while(numread > 0);
	seconds = time(NULL);
        while(copied = recv(sock, buf, sizeof(buf), MSG_DONTWAIT)){
                if(copied == 0)break;
                if(copied > 0){
                        fwrite(buf, 1, copied, file);
                        int i;
                        for(i=0;i<4096;i++)buf[i]=0;
                        seconds = time(NULL);
                } else if((time(NULL) - seconds) >= 1){
                        break;
                }
        }
	close(sock);
	
	//receive part 3
	sprintf(str, msg, partNames[2][0], uname, pass);

	getHostInfo(parts[2][0], addr, port);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((unsigned short)atoi(port));
	sin.sin_addr.s_addr = inet_addr(addr);
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	connect(sock, (struct sockaddr*)&sin, sizeof(sin));
	
	numread = strlen(str);
	bufptr = str;
	do{
		int numsent = send(sock, bufptr, numread, MSG_NOSIGNAL);
		if(numsent < 0){
			break;
		}
		bufptr += numsent;
		numread -= numsent;
	}while(numread > 0);
	
	recv(sock, buf, sizeof(buf), 0);
	for(i=0; i<4096; i++)buf[i]=0;
	
	numread = strlen(ack);
	bufptr = ack;
	do{
		int numsent = send(sock, bufptr, numread, MSG_NOSIGNAL);
		if(numsent < 0){
			break;
		}
		bufptr += numsent;
		numread -= numsent;
	}while(numread > 0);
	seconds = time(NULL);
        while(copied = recv(sock, buf, sizeof(buf), MSG_DONTWAIT)){
                if(copied == 0)break;
                if(copied > 0){
                        fwrite(buf, 1, copied, file);
                        int i;
                        for(i=0;i<4096;i++)buf[i]=0;
                        seconds = time(NULL);
                } else if((time(NULL) - seconds) >= 1){
                        break;
                }
        }
	close(sock);
	for(i=0;i<2048;i++){
		addr[i] = 0;
		port[i] = 0;
	}
	//receive part 4
	sprintf(str, msg, partNames[3][0], uname, pass);

	getHostInfo(parts[3][0], addr, port);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((unsigned short)atoi(port));
	sin.sin_addr.s_addr = inet_addr(addr);
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	connect(sock, (struct sockaddr*)&sin, sizeof(sin));
	
	numread = strlen(str);
	bufptr = str;
	do{
		int numsent = send(sock, bufptr, numread, MSG_NOSIGNAL);
		if(numsent < 0){
			break;
		}
		bufptr += numsent;
		numread -= numsent;
	}while(numread > 0);
	
	recv(sock, buf, sizeof(buf), 0);
	for(i=0; i<4096; i++)buf[i]=0;
	
	numread = strlen(ack);
	bufptr = ack;
	do{
		int numsent = send(sock, bufptr, numread, MSG_NOSIGNAL);
		if(numsent < 0){
			break;
		}
		bufptr += numsent;
		numread -= numsent;
	}while(numread > 0);
	seconds = time(NULL);
        while(copied = recv(sock, buf, sizeof(buf), MSG_DONTWAIT)){
                if(copied == 0)break;
                if(copied > 0){
                        fwrite(buf, 1, copied, file);
                        int i;
                        for(i=0;i<4096;i++)buf[i]=0;
                        seconds = time(NULL);
                } else if((time(NULL) - seconds) >= 1){
                        break;
                }
        }
	close(sock);
	for(i=0;i<2048;i++){
		addr[i] = 0;
		port[i] = 0;
	}
	fclose(file);
}
