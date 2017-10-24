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

typedef enum http_meth {GET, POST, HEAD, UNKNOWN} http_method;

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
	int status;
	int content_length;
} http_request;

// global variables
int buff_size = 1;

void error(char *msg){
	perror(msg);
	exit(1);
}

void error_thread(char* msg){
	fprintf(stderr, "%s", msg);
}

void start_server(char *address, char *port){

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
		if(newsockfd < 0){
			error_thread("ERROR on accept");
		}
		else{		
			// create a new pthread
			pthread_t thread;
			if(pthread_create(&thread, NULL, handle_client, &newsockfd)){
				error_thread("ERROR creating pthread");
			}
			else printf("Waiting for more connections.\n");
		}
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
			if(n <= 0){
				connected = false;			
				break;
			}	

			if(*buffer == '\n' && check_newline){		
			
				char **new = malloc(sizeof(char *) * (line_num + 1));
				if(!new){
					error_thread("ERROR on malloc new");
					connected = false;
					break;				
				}
				for(int i = 0; i < line_num+1; i++){
					new[i] = NULL;
				}		
				if(lines)memcpy(new, lines, (sizeof(char *) * line_num));
	
				if(lines)free(lines);
	
				new[line_num] = backlog;
				lines = new;
				line_num++;		

				if(!backlog){
					http_read = true;
					backlog = NULL;
					backlog_size = 1;
					check_newline = false;
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
				if(!new){
					error_thread("ERROR on malloc new");
					connected = false;
					break;				
				}
				bzero(new, (backlog_size + 1));
				
				if(backlog)strcpy(new, backlog);	
	
				if(backlog)free(backlog);			

				new[backlog_size-1] = *buffer;
				
				backlog = new;
				backlog_size++;
			}
		}		
		
		if(lines){
			char** start_lines = &lines[0];
			while(!lines[0]){
				free(lines[0]);
				lines = &lines[1];
				line_num--;	
			}

			http_request *req = parse_http(lines, line_num-1);				
			if(req){
				print_http(req);			
				handle_req(socket, req);				
				free_http(req);				
			}
			else{
				error_thread("ERROR parsing http req");
				connected = false;		
			}
			free_lines(lines, line_num);	
			free(start_lines);
		}
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

	for(int i = 0; i < initial_length; i++){
		if(initial[i] == ' '){
			if(strcmp(acc, "GET") == 0){
				method = GET;
				req->status = 500;
				break;
			}
			else if(strcmp(acc, "POST") == 0){
				method = POST;
				req->status = 500;
				break;
			}
			else if(strcmp(acc, "HEAD") == 0){
				method = HEAD;
				req->status = 500;
				break;
			}
			else{
				req->status = 400;
				method = UNKNOWN;
			} 
		}
		else{
			char* new = calloc(length + 1, sizeof(char));
			if(!new){
				error_thread("ERROR on calloc new");
				return NULL;			
			}
			if(acc)strcpy(new, acc);
			new[length-1] = initial[i];
			if(acc)free(acc);			
			acc = new;
			length++;
		}
	}

	req->method = method;

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
			if(!new){
				error_thread("ERROR on calloc new");
				return NULL;			
			}
			if(acc)strcpy(new, acc);
			new[length-1] = initial[i];			
			if(acc)free(acc);			
			acc = new;
			length++;
		}
	}

	int path_length = strlen(acc);

	req->version = atof(&initial[method_length + path_length + 7]);
	
	http_header** headers = malloc(sizeof(http_header) * (line_num-1));
	if(!headers){
		error_thread("ERROR on malloc headers");
		return NULL;			
	}

	for(int i = 0; i < line_num-1; i++){
		headers[i] = NULL;
	}

	int pointer = 0;	

	for(int i = 1; i < line_num; i++){
		http_header* head = malloc(sizeof(http_header));
		if(!head){
			error_thread("ERROR on malloc head");
			return NULL;			
		}
		
		head->name = strtok(lines[i], ":");
		head->value = strtok(NULL, ":");
	
		headers[pointer] = head;
		pointer++;
	}
	req->headers = headers;
	req->header_num = line_num-1;
	req->content_length = 0;
	return req;
}

void handle_req(int socket, http_request *req){

	int file_length;
	char* file = get_file(req);
	if(file)file_length = req->content_length;
	else{
		file_length = 0;
	}

	char* version = "HTTP/1.1";

	// nothing bad has happened to the response internally
	if(req->status == 500)req->status = 200;

	char* status = get_status(req->status);
	char* file_type = get_file_type(req);

	int total_length = strlen(version) + sizeof(int) + strlen(status) + strlen("\r\nContent-Type: ") + strlen(file_type) + strlen("\r\nContent-Length: ") + sizeof(int) + strlen("\r\n\r\n") + 3;

	printf("\nGot total length %d", total_length);
	char* buff = calloc(total_length + 1, sizeof(char));
	if(!buff){
		error_thread("ERROR on calloc buff");
		return;			
	}


	sprintf(buff, "%s %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", version, req->status, status, file_type, file_length);
 
	printf("\n\nHTTP Response Details ========== \n%s\n", buff);
	write(socket, buff, total_length);
	write(socket, file, file_length);
	
	if(file)free(file);
	if(buff)free(buff);
}

char* get_status(int status){
	switch(status){
	case 200: return "OK";
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	default: return "Internal Server Error";
	}
}

char* get_file_type(http_request *req){

	char* path = req->path;

	char* ext = strchr(path, '.');
	if(!ext) return "text/html";

	if(strcmp(ext, ".txt") == 0){
		return "text/plain";
	}
	else if(strcmp(ext, ".png") == 0){
		return "image/png";
	}
	else return "text/html";
}

char* get_file(http_request *req){

	// bad request	
	if(req->status == 400){
		free(req->path);
		req->path = "pages/400.html";
	}

	char* path = req->path;
	int pos = strcspn(req->path, "/");
	if(pos == 0)path = &path[1];

	FILE *fp;

	// if forbidden path = 403
	if(strstr(path, "../")){
		req->status = 403;
		free(req->path);
		req->path = "pages/403.html";
		fp = fopen(req->path, "rb");
		if(!fp){
			return NULL;
		}
	}
	else{
	
		if(strlen(path) == 0){
			free(req->path);
			req->path = "pages/index.html";
			fp = fopen(req->path, "rb");
				
		}
		else fp = fopen(path, "rb");
		// if file cannot be found = 404		
		if(!fp){
			req->status = 404;
			free(req->path);
			req->path = "pages/404.html";
			fp = fopen(req->path, "rb");
		}
		if(!fp){
			req->status = 404;
			return NULL;
		}
	}

	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);	

	char *buff = calloc(size+1, sizeof(char));
	if(!buff){
		error_thread("ERROR on calloc buff");
		return NULL;			
	}

	fread(buff, sizeof(char), size, fp);

	req->content_length = size;
	fclose(fp);
	return buff;		
}

void free_lines(char** lines, int line_num){
	for(int i = 0; i < line_num; i++){
		if(lines[i])free(lines[i]);
	}
}

void free_http(http_request *req){
	printf("\nFreeing %s\n", req->path);	
	if(strcspn(req->path, "/") == 0)free(req->path);
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
