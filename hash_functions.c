#include <stdio.h>
// Add your system includes here.

#include "ftree.h"
#include "hash.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BLOCK_SIZE 8

char *hash(FILE *file) {
	char *hash_val = malloc(sizeof(char)*8);
	char buffer[1];
	// set hash_val all to '\0'
	for(int i = 0; i < 8; i++){
		hash_val[i] = '\0';
	}
	int i = 0;
	//Read all characters from file
	while(fread(buffer, 1, 1, file) == 1){
		hash_val[i] = buffer[0]^hash_val[i];
		i++;
		if(i >= MAX_BLOCK_SIZE){
			i = 0;
		}
	}
	return hash_val;
}
