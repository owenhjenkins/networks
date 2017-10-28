#include <stdbool.h>
#include <openssl/ssl.h>

// structures/enum needed
typedef enum http_meth http_method;
typedef struct http_head http_header;
typedef struct http_req http_request;

// SERVER FUNCTIONAL METHODS

// start the server
void start_server(char*, char*); 

// on a thread, handle a client
void* handle_client(void*);

// parse an http request
http_request* parse_http(char**, int);

// serve a resource requested
void handle_req(SSL*, http_request*);

// get a resource from a request
char* get_file(http_request*);




// AUXILIARY METHODS

// add the file directory to a path
char* append_dir(char*, bool);

// get status string given number
char* get_status(int);

// get the type of a requested resource
char* get_file_type(http_request*);

// get the string representation of an http method enum
char* method_str(http_method);




// DEBUG METHODS

// print an array of char arrays
void print_lines(char**, int);

// print an http structure
void print_http(http_request*);

// error on server
void error(char*);

// error on thread
void error_thread(char*);




// MEMORY MANAGEMENT METHODS

// free an array of char arrays
void free_lines(char**, int);

// free an http structure
void free_http(http_request*);
