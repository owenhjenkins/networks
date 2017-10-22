#include <stdio.h>
#include <server.h>

int main(int argc, char *argv[]){ 

	if(argc != 3){
		fprintf(stderr, "usage: %s address portnumber\n", argv[0]);
		return 1;
	}
	else{
		display_incoming(argv[1], argv[2]);
		return 0;
	}
}
