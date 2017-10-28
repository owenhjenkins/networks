#include <stdio.h>
#include "server.h"

int main(int argc, char *argv[]){ 

	if(argc != 3){
		fprintf(stderr, "usage: %s <file-directory> <portnumber>\n\nfile-directory: where the server resources are located.\nportnumber: port to bind server to.\n", argv[0]);
		return 1;
	}
	else{
		start_server(argv[1], argv[2]);
		return 0;
	}
}
