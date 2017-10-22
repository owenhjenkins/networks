#include <stdio.h>
#include <server.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>

typedef enum http_meth {GET, POST, HEAD} http_method;

typedef struct http_head {
	char* name;
	char* value;
} http_header;

typedef struct http_req {
	http_method method;
	char* path;
	float version;
	int header_num;
	http_header** headers;
	//char* body;
} http_request;

// global variables
int buff_size = 1;

void error(char *msg){
	perror(msg);
	exit(1);
}

void display_incoming(char *address, char *port){

	printf("Attempting to initialize server at address %s.\n",  address);

	// sockfd, newsockfd - file descriptors to store the locations of the sockets
	// clilen - the size of the address of the client
	// n - number of characters read or written
	int sockfd;

	int portno = atoi(port);

	// structures to store the server and client addresses 
	struct sockaddr_in6 serv_addr;

	// Create a socket
	// Socket takes three arguments:
	// address domain - AF_INET for internet addresses	
	// type of socket - SOCK_STREAM for continuous stream
	// protocol - 0 for auto choosing the most appropriate	
	sockfd = socket(AF_INET6, SOCK_STREAM, 0);
	if(sockfd < 0) error("ERROR opening socket");	

	// set socket options to resuable after we quit
	int tru = 1;
	int opt = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tru, sizeof(int));
	if(opt < 0)error("ERROR setting socket options.");	

	// initialize the whole server address to zero
	bzero((char*) &serv_addr, sizeof(serv_addr));

	serv_addr.sin6_family = AF_INET6; 

	// convert port number to host byte order
	serv_addr.sin6_port = htons(portno);

	// gets the ip address of the machine this is running on
	if(inet_pton(AF_INET6, address, serv_addr.sin6_addr.s6_addr) < 0)
		error("ERROR on setting server address");

	printf("Attempting to bind to port %d.\n", portno);

	// Bind the socket to an address (port number)
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");	

	printf("Successfully bound. Listening for a connection.\n");

	// Listen for connections (5 is the standard backlog queue to use)
	listen(sockfd, 5);

	// structure to store the client address
	struct sockaddr_in6 cli_addr;
	int clilen = sizeof(cli_addr);

	while(true){

		// block until a client connects
		int newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
		if(newsockfd < 0) error("ERROR on accept");
		
		// create a new pthread
		pthread_t thread;
		if(pthread_create(&thread, NULL, handle_client, &newsockfd)){
			error("ERROR creating pthread");
		}
		printf("Waiting for more connections.\n");
	}	
}

// called when a new pthread is created
void *handle_client(void *param){
	
	int socket = *(int*)param;

	// buffer to store characters read
	char buffer[1];

	int line_num = 0;
	char *backlog = NULL;	
	int backlog_size = 1;

	bool check_newline = false;

	int result;
	bool connected = true;
	while(connected){

		char **lines = NULL;
		bool http_read = false;
		while(!http_read){
	
			// use the new fd, block until the client sends characters
			// read either the total number of characters, or buff_size
			// return the number of characters read, in the buffer	
			int n = read(socket, buffer, buff_size);
			if(n < 0){
				printf("\nERROR reading from socket, thread closed.");
				result = 1;
				pthread_exit(&result);
			}
			else if(n == 0)connected = false;
	
			//printf("\nCONSIDERING %c", *buffer);

			if(*buffer == '\n' && check_newline){		
			
				char **new = malloc(sizeof(char *) * (line_num + 1));
				if(!new)error("ERROR on malloc new");
				for(int i = 0; i < line_num+1; i++){
					new[i] = NULL;
				}		
				if(lines)memcpy(new, lines, (sizeof(char *) * line_num));
	
				if(lines)free(lines);
	
				new[line_num] = backlog;
				lines = new;
				line_num++;			
				
				//printf("\nBacklog %s", backlog);	

				if(!backlog){
					http_read = true;
					break;				
				}

				backlog = NULL;
				backlog_size = 1;
				check_newline = false;
			} 
			else if(*buffer == '\r'){
				check_newline = true;
			}
			else{				
				char *new = malloc(sizeof(char) * (backlog_size + 1));
				if(!new)error("ERROR on malloc new");
				bzero(new, (backlog_size + 1));
				
				if(backlog)strcpy(new, backlog);	
	
				if(backlog)free(backlog);			

				new[backlog_size-1] = *buffer;
				
				backlog = new;
				backlog_size++;
			}
		}
		
		if(lines[0]){
			printf("\nReceived HTTP Request:");
			//print_lines(lines, line_num-1);
			//printf("\nParsing...");
			http_request *req = parse_http(lines, line_num-1);
			handle_req(socket, req);	
			print_http(req);			
			free_http(req);
		}
		free_lines(lines, line_num);
		free(lines);
	}			

	// close the socket when a client has disconnected
	close(socket);
	printf("\n[%d | client disconnected]\n", (int)pthread_self());
	result = 0;

	// exit the thread, making sure storage used can be reclaimed
	pthread_detach(pthread_self());
	pthread_exit(&result);
}

http_request* parse_http(char** lines, int line_num){
	
	http_request *req = malloc(sizeof(http_request));

	char* initial = lines[0];
	char* acc = NULL;
	int length = 1;
	http_method method;		
	int initial_length = (int)strlen(initial);

	//printf("\nsize = %d", initial_length);

	for(int i = 0; i < initial_length; i++){
		if(initial[i] == ' '){
			if(strcmp(acc, "GET") == 0){
				method = GET;
				break;
			}
			else if(strcmp(acc, "POST") == 0){
				method = POST;
				break;
			}
			else if(strcmp(acc, "HEAD") == 0){
				method = HEAD;
				break;
			}
			else error("ERROR non-valid http method");
		}
		else{
			char* new = calloc(length + 1, sizeof(char));
			if(!new)error("ERROR on calloc new");
			if(acc)strcpy(new, acc);
			new[length-1] = initial[i];
			if(acc)free(acc);			
			acc = new;
			length++;
		}
	}

	req->method = method;
	//printf("\nGot method as %s", method_str(req->method));

	int method_length = strlen(acc);
	free(acc);
	acc = NULL;
	length = 1;

	for(int i = method_length + 1; i < initial_length; i++){
		if(initial[i] == ' '){
			req->path = acc;
			break;
		}
		else{
			char* new = calloc(length + 1, sizeof(char));
			if(!new)error("ERROR on calloc new");
			if(acc)strcpy(new, acc);
			new[length-1] = initial[i];			
			if(acc)free(acc);			
			acc = new;
			length++;
		}
	}

	//printf("\nGot path as %s", req->path);

	int path_length = strlen(acc);

	req->version = atof(&initial[method_length + path_length + 7]);

	//printf("\nGot version as %.1f", req->version);
	
	http_header** headers = malloc(sizeof(http_header) * (line_num-1));
	if(!headers)error("ERROR on malloc headers");

	for(int i = 0; i < line_num-1; i++){
		headers[i] = NULL;
	}

	int pointer = 0;	

	for(int i = 1; i < line_num; i++){
		http_header* head = malloc(sizeof(http_header));
		if(!head)error("ERROR on malloc head");
		
		head->name = strtok(lines[i], ":");
		head->value = strtok(NULL, ":");
	
		headers[pointer] = head;
		pointer++;
	}
	req->headers = headers;
	req->header_num = line_num-1;
	return req;
}

void handle_req(int socket, http_request *req){
	char* path = req->path;
	char buff[8192];
	bzero(buff, 8192);
	char file[10000];
	bzero(file, 10000);
	FILE *fp = fopen("index.html", "r");
	fread(file, sizeof(char), 1000, fp);	

	sprintf(buff, "HTTP/1.1 200 OK\r\nDate: Sun, 22 Oct 2017 00:00:00 BST\r\nContent-Type: text/html\r\nContent-Length: 1000\r\n\r\n%s", file);

	printf("\nSending: %s", buff);
	write(socket, buff, 1000);
}

void free_lines(char** lines, int line_num){
	for(int i = 0; i < line_num; i++){
		if(lines[i])free(lines[i]);
	}
}

void free_http(http_request *req){
	free(req->path);
	for(int i = 0; i < req->header_num; i++){
		free(req->headers[i]);
	}	
	free(req->headers);
	free(req);
}

void print_lines(char** lines, int line_num){
	if(!lines)printf("\n	EMPTY");
	for(int i = 0; i < line_num; i++){
		printf("\n	[line %d] %s", i, lines[i]);
	}
}

void print_http(http_request *req){
	printf("\n\nHTTP Request Details ==========");
	printf("\nMethod: %s", method_str(req->method));
	printf("\nPath:   %s", req->path);
	printf("\nVer:    %.1f", req->version);
	printf("\nHeaders:");
	for(int i = 0; i < req->header_num; i++){
		http_header** headers = req->headers;		
		http_header* header = headers[i];
		printf("\n -- %s : %s", header->name, header->value);	
	}
	printf("\n");
}

char *method_str(http_method meth){
	switch(meth){
		case GET: return "GET";
		case POST: return "POST";
		case HEAD: return "HEAD";
		default: return "INVALID";
	}
}
