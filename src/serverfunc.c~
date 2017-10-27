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

#include <openssl/ssl.h>
#include <openssl/err.h>

// http method enum
typedef enum http_meth {GET, POST, HEAD, UNKNOWN} http_method;

// http header structure
typedef struct http_head {
	char* name;
	char* value;
} http_header;

// http request structure
typedef struct http_req {
	http_method method;			// method
	char* path;				// requested resource
	float version;				// http version
	int header_num;				// number of headers
	http_header** headers;			// attached headers
	int status;				// status of request (for the response)
	int content_length;			// content length of resource requested
} http_request;

// global variables
char* dir = NULL;				// directory of the web files
SSL_CTX *ctx;					// ssl context

///
/// critical error, stop the server
///
void error(char *msg){
	perror(msg);
	exit(1);
}

///
/// error on one of the client threads
///
void error_thread(char* msg){
	fprintf(stderr, "%s", msg);
}

///
/// begin the server, using the directory given for resources and the port number given
///
void start_server(char* directory, char *port){

	// initialize ssl and create the ssl context
	OpenSSL_add_ssl_algorithms();
	SSL_load_error_strings();
	SSL_library_init();
	const SSL_METHOD *method;
	method = SSLv23_server_method();
	ctx = SSL_CTX_new(method);
	if(!ctx) {
	       	error("ERROR creating SSL context");
    	}

	// set up the private key and certificates to use
	if(SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM) <= 0){
		error("ERROR loading certificate file");
	}
	if(SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM) <= 0){
		error("ERROR loading key file (wrong password?)");
	}

	dir = directory;

	printf("Attempting to initialize server at port %s.\n",  port);

	// sockfd, newsockfd - file descriptors to store the locations of the sockets
	int sockfd;

	int portno = atoi(port);

	// structure to store the server address
	struct sockaddr_in6 serv_addr;

	// Create a socket
	// Socket takes three arguments:
	// address domain - AF_INET for internet addresses	
	// type of socket - SOCK_STREAM for continuous stream
	// protocol - 0 for auto choosing the most appropriate	
	sockfd = socket(AF_INET6, SOCK_STREAM, 0);
	if(sockfd < 0){
		error("ERROR creating socket");	
	}
	bzero((char*) &serv_addr, sizeof(serv_addr));

	serv_addr.sin6_family = AF_INET6; 

	// convert port number to host byte order
	serv_addr.sin6_port = htons(portno);
	serv_addr.sin6_family = AF_INET6;
	serv_addr.sin6_addr = in6addr_any;

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

///
/// called when a new pthread is created
///
void *handle_client(void *param){
	
	// initialize the ssl structure and use the context
	SSL *ssl;
	ssl = SSL_new(ctx);
	if(!ssl){
		error("ERROR creating ssl struct");
	}

	// get the socket from the parameters
	int socket = *(int*)param;

	if(SSL_set_fd(ssl, socket) != 1){
		error("ERROR setting ssl to socket fd");
	}

	if(SSL_accept(ssl) > 0){

		// buffer to store characters read
		char buffer[1];
	
		int line_num = 0;
		char *backlog = NULL;	
		int backlog_size = 1;
	
		bool check_newline = false;
	
		int result;	
	
		// while connected, initialize lines to store any request
		char **lines = NULL;	
		bool http_read = false;
		while(!http_read){

			// use the new fd, block until the client sends characters
			// read either the total number of characters, or buff_size
			// return the number of characters read, in the buffer	
			int n = SSL_read(ssl, buffer, 1);		
			if(n < 0){
				error_thread("ERROR on read");
				break;
			}			
			else if(n == 0){			
				break;
			}	

			if(*buffer == '\n' && check_newline){		
	
				// we have reached the end of a header line,
				// initialize a new line and store the characters 
				// read since the last line			
				char **new = malloc(sizeof(char *) * (line_num + 1));
				if(!new){
					error_thread("ERROR on malloc new");
					break;				
				}
				for(int i = 0; i < line_num+1; i++){
					new[i] = NULL;
				}		
				if(lines)memcpy(new, lines, (sizeof(char *) * line_num));
	
				if(lines)free(lines);	
				new[line_num] = backlog;//new_line;
				lines = new;
				line_num++;		

				if(!backlog){
						
					backlog_size++;
	
					// we have read the entire header
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
				// we expect a LF next to signify a new line
				check_newline = true;
			}
			else{			
				// make the buffer bigger and store the read character
				char *new = malloc(sizeof(char) * (backlog_size + 1));
				if(!new){
					error_thread("ERROR on malloc new");
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
	
			// we got lines, so parse the http request
			char** start_lines = &lines[0];
			while(!lines[0]){
				free(lines[0]);
				lines = &lines[1];
				line_num--;	
			}
	
			http_request *req = parse_http(lines, line_num-1);				
			if(req){
				// if we successfully parsed a request, handle it				
				print_http(req);			
				handle_req(ssl, req);				
				free_http(req);				
			}
			else{
				error_thread("ERROR parsing http req");		
			}
			free_lines(lines, line_num);	
			free(start_lines);
		}
		
	
		// close the socket when the request has been read
		SSL_free(ssl);	
		close(socket);
		printf("\n[%d | client disconnected]\n", (int)pthread_self());
		result = 0;
	
		// exit the thread, making sure storage used can be reclaimed
		pthread_detach(pthread_self());
		pthread_exit(&result);
	} 
	else {
		ERR_print_errors_fp(stderr);
		error("ERROR on ssl accept (not using https?)");
	}
}

///
/// given a list of lines and a number of lines, attempt to parse an http request
///
http_request* parse_http(char** lines, int line_num){
	
	http_request *req = malloc(sizeof(http_request));

	char* initial = lines[0];
	char* acc = NULL;
	int length = 1;
	http_method method;		
	int initial_length = (int)strlen(initial);

	// read the http method
	for(int i = 0; i < initial_length; i++){
		if(initial[i] == ' '){

			// we reached the end of the method section
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
		
			// not at the end of the method section yet, carry on reading chars
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

	// read the resource path
	for(int i = method_length + 1; i < initial_length; i++){
		if(initial[i] == ' '){
		
			// we reached the end of the path
			req->path = acc;
			break;
		}
		else{

			// not at the end of the path yet, carry on reading chars
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

	// extract the version number from the remaining characters in the first line
	req->version = atof(&initial[method_length + path_length + 7]);
	
	// add the headers to the request, parsed into structures
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

		// create a new structure for each header line
		http_header* head = malloc(sizeof(http_header));
		if(!head){
			error_thread("ERROR on malloc head");
			return NULL;			
		}
		
		// get the name and value of each line
		printf("\nLINES[I] = %s\n", lines[i]);
		head->name = strtok(lines[i], ":");
		head->value = strtok(NULL, ":");
	
		headers[pointer] = head;
		pointer++;
	}

	// set some remaining fields and return the parsed request object
	req->headers = headers;
	req->header_num = line_num-1;
	req->content_length = 0;
	return req;
}

///
/// given an http request object, handle it and serve the resource
///
void handle_req(SSL *ssl, http_request *req){

	// get some info about the resource for the response header
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

	// calculate the size of the buffer needed
	int total_length = strlen(version) + sizeof(int) + strlen(status) + strlen("\r\nContent-Type: ") + strlen(file_type) + strlen("\r\nContent-Length: ") + sizeof(int) + strlen("\r\n\r\n") + 3;

	char* buff = calloc(total_length + 1, sizeof(char));
	if(!buff){
		error_thread("ERROR on calloc buff");
		return;			
	}

	// construct the http response string
	sprintf(buff, "%s %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", version, req->status, status, file_type, file_length);
 
	printf("\n\nHTTP Response Details ========== \n%s\n", buff);
	
	// write the response header and resource
	SSL_write(ssl, buff, total_length);
	SSL_write(ssl, file, file_length);
	
	// free the used memory
	if(file)free(file);
	if(buff)free(buff);
}

///
/// given a status code, return an information string
///
char* get_status(int status){
	switch(status){
	case 200: return "OK";
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	default: return "Internal Server Error";
	}
}

///
/// get the type of a file requested by an http request
///
char* get_file_type(http_request *req){

	char* path = req->path;

	// get the file extension from the end of the string
	char* ext = strrchr(path, '.');
	if(!ext) return "text/html";

	// return the appropriate content type
	if(strcmp(ext, ".txt") == 0){
		return "text/plain";
	}
	else if(strcmp(ext, ".png") == 0){
		return "image/png";
	}
	else return "text/html";
}

///
/// append the given directory to the start of a resource path
/// if free_path is true, also free the old path as this memory was previously alloc'd
///
char* append_dir(char* path, bool free_path){
	if(dir){
		int dir_length = strlen(dir);
		int path_length = strlen(path);
	
		// create a new bigger buffer to store both strings
		char* new = calloc(dir_length + 2 + path_length, sizeof(char));
		if(!new){
			error_thread("ERROR on calloc new");
			return NULL;			
		}

		// combine the strings and return the result
		sprintf(new, "%s/%s", dir, path);
		if(free_path)free(path);
		return new;
	}
	else{
		return path;
	}
}

///
/// given an http request, return the resource requested
///
char* get_file(http_request *req){

	FILE *fp;

	// bad request	
	if(req->status == 400){
		free(req->path);
		req->path = append_dir("/400.html", false);
	}
	else{

		// add the file directory given when server was started
		req->path = append_dir(req->path, true);
		char* path = req->path;
	
		// if forbidden path = 403
		if(strstr(path, "../")){
			req->status = 403;

			free(req->path);
			
			// replace the path with 403 file
			req->path = append_dir("/403.html", false);
			path = req->path;
			
			// remove any '/'s from the start
			int i = 0;			
			while(path[i] == '/'){
				path = &path[i+1];
			}
			fp = fopen(path, "rb");
			
			if(!fp){
				// no 403 file
				return NULL;
			}
		}
		else if(path[strlen(path)-1] == '/'){

			// replace the path with the index file
			char* new = calloc(strlen(path) + 2 + strlen("index.html"), sizeof(char));
			if(!new){
				error_thread("ERROR on calloc new");
				return NULL;			
			}				
			sprintf(new, "%s/%s", path, "index.html");
			free(req->path);
			req->path = new;
			path = req->path;

			// remove any '/'s from the start	
			int i = 0;			
			while(path[i] == '/'){
				path = &path[i+1];
			}

			// open the file
			fp = fopen(path, "rb");
	
			if(!fp){
				
				// doesn't exist, so
				// replace the path with 404 file
				req->status = 404;
				free(req->path);
				req->path = append_dir("/404.html", false);
				path = req->path;

				// remove any '/'s from the start
				int i = 0;			
				while(path[i] == '/'){
					path = &path[i+1];
				}
				fp = fopen(path, "rb");
			}
			if(!fp){
				// no 404 file
				return NULL;
			}
		}
		else {
			// remove any '/'s from the start
			int i = 0;
			while(path[i] == '/'){
				path = &path[i+1];
			}
			fp = fopen(path, "rb");

			// if file cannot be found = 404		
			if(!fp){

				// replace the path with 404 file
				req->status = 404;
				free(req->path);
				req->path = append_dir("/404.html", false);
				path = req->path;

				// remove any '/'s from the start
				int i = 0;			
				while(path[i] == '/'){
					path = &path[i+1];
				}
				fp = fopen(path, "rb");
			}
			if(!fp){
				// no 404 file
				return NULL;
			}
		}
	}

	// get the size of the file
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek(fp, 0, SEEK_SET);	

	// create a buffer of that size
	char *buff = calloc(size+1, sizeof(char));
	if(!buff){
		error_thread("ERROR on calloc buff");
		return NULL;			
	}

	// read the file into the buffer
	fread(buff, sizeof(char), size, fp);

	// set the content length and close the file
	req->content_length = size;
	fclose(fp);

	return buff;		
}

///
/// free an array of char arrays
///
void free_lines(char** lines, int line_num){
	for(int i = 0; i < line_num; i++){
		if(lines[i])free(lines[i]);
	}
}

///
/// free an http request structure
///
void free_http(http_request *req){	
	free(req->path);
	for(int i = 0; i < req->header_num; i++){
		free(req->headers[i]);
	}	
	free(req->headers);
	free(req);
}

///
/// print an array of char arrays
///
void print_lines(char** lines, int line_num){
	if(!lines)printf("\n	EMPTY");
	for(int i = 0; i < line_num; i++){
		printf("\n	[line %d] %s", i, lines[i]);
	}
}

///
/// print an http structure
/// 
void print_http(http_request *req){
	printf("\n\nHTTP Request Details ==========");
	//printf("\nMethod: %s", method_str(req->method));
	//printf("\nPath:   %s", req->path);
	//printf("\nVer:    %.1f", req->version);
	//printf("\nHeaders:");
	for(int i = 0; i < req->header_num; i++){
		http_header** headers = req->headers;		
		http_header* header = headers[i];
	//	printf("\n -- %s : %s", header->name, header->value);	
	}
	printf("\n");
}

///
/// get the string representation of an http method enum
///
char *method_str(http_method meth){
	switch(meth){
		case GET: return "GET";
		case POST: return "POST";
		case HEAD: return "HEAD";
		default: return "INVALID";
	}
}
