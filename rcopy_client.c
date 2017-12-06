#include <stdio.h>
#include "ftree.h"

	#ifndef PORT
#define PORT 30000
#endif
int main(int argc, char** argv){

	int err = -1;

    if (argc != 4) {    
        printf("Usage:\n\tfcopy SRC DEST IPAD\n");
        return err;
    }

    fcopy_client(argv[1], argv[2], argv[3], PORT);
    return 0;
}
