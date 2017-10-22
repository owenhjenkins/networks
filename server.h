typedef enum http_meth http_method;
typedef struct http_head http_header;
typedef struct http_req http_request;

void error(char*);
void display_incoming(char*, char*); 
void* handle_client(void*);
http_request* parse_http(char**, int);
void init_lines(char**, int);
void free_lines(char**, int);
void free_http(http_request*);
void print_lines(char**, int);
void print_http(http_request*);
char* method_str(http_method);

void handle_req(int, http_request*);
