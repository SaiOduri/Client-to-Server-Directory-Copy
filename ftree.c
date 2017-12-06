#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <libgen.h>
#include "ftree.h"
#include "hash.h"
 
#define PERM_VAL 0777   

// function prototypes
int send_server(char *source, char *destination, int sockt);

void fcopy_server(int port);

int setup(int port);
int make_dir(int destinationperm, int copyperm, struct stat destinationfile, char *newpath);
int make_file(int destinationperm, int copyperm, struct stat destinationfile, char *newpath, char destinationhash[HASH_SIZE], struct fileinfo *head);

int fcopy_client(char *source, char *destination, char *host, int port){
    int check_err;
    // initialize socket, if error: exit
    int sockt = socket(AF_INET, SOCK_STREAM, 0);
    if (sockt < 0) {
        perror("socket");
        exit(1);
    }

    // sockaddr setups and connecting the sockets
    struct sockaddr_in mate;
    mate.sin_family = AF_INET;
    mate.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &mate.sin_addr) < 1) {
        perror("inet_pton");
        close(sockt);
        exit(1);
    }
    if (connect(sockt, (struct sockaddr *)&mate, sizeof(mate)) < -1) {
        perror("connect");
        close(sockt);
        exit(1);
    }


    check_err = send_server(source, destination, sockt);
    close(sockt);
    return check_err;
}

void fcopy_server(int port){
	// initialize variables
    int listener, fd;
    socklen_t socklen;
    struct sockaddr_in mate;
    listener = setup(port);
    socklen = sizeof(mate);

    // asssign space to structs 
    char *destination = malloc(MAXPATH*sizeof(char *));
    char destinationhash[HASH_SIZE];
    struct stat destinationfile;
    struct fileinfo *head = malloc(sizeof(struct fileinfo));
    char *name = malloc(sizeof(char *));
	char *newpath = malloc(MAXPATH*sizeof(char *));

    int check_err = MATCH;
    int trans = TRANSMIT_OK; 
    int nbytes;
    int tru = 1;
    int destinationperm = 0;
    int copyperm;
    int fsize;

    mode_t mode;
    size_t size;

    
    // main while loop
    while(tru){
        if ((fd = accept(listener, (struct sockaddr *)&mate, &socklen)) < 0) {
            perror("accept");
        } 
        else {
		    printf("New connection high port %d\n", ntohs(mate.sin_port));
		    //if there are files, continue reading destination
		    while((nbytes = read(fd, destination, MAXPATH*sizeof(char *))) > 0){
		    	// Error checking of structs and receiving structs
		        if((nbytes = read(fd, head->path, MAXPATH*sizeof(char))) < 0){
		            perror("server: path read");
		            check_err = MATCH_ERROR;
		            write(fd, &check_err, sizeof(int));
		            continue;
		        }
			    if((nbytes = read(fd, &mode, sizeof(mode_t))) < 0){
		            perror("server: mode read");
		            check_err = MATCH_ERROR;
		            write(fd, &check_err, sizeof(int));
		            continue;   
		        }
		        if((nbytes = read(fd, head->hash, HASH_SIZE*sizeof(char))) < 0){
		            perror("server: hash read");
		            check_err = MATCH_ERROR;
		            write(fd, &check_err, sizeof(int));
		            continue;                     
		        }
		        if((nbytes = read(fd, &size, sizeof(size_t))) < 0){
		            perror("server: size read");
		            check_err = MATCH_ERROR;
		            write(fd, &check_err, sizeof(int));
		            continue;                                                               
		        }
		        head->mode = ntohl(mode);
		        head->size = ntohl(size);

		        check_err = MATCH;
		        destinationhash[0] = '\0';
		        if (lstat(destination, &destinationfile) != -1){
		            if(S_ISREG(destinationfile.st_mode)){
		                check_err = MATCH_ERROR;
		                write(fd, &check_err, sizeof(int));
		                continue;
		            }
		        }

		        // obtain name from directory/file
		        name = head->path;
			    if(strrchr(head->path, '/') != NULL){
			        name = strrchr(head->path, '/');
			        name = name + 1;
			    }
		        snprintf(newpath, MAXPATH, "%s/%s", destination, name);
                copyperm = head->mode & PERM_VAL;

		        // if it is file:
		        if(S_ISREG(head->mode)){
		            // check if the file errors/matches/mismatches
		            check_err = make_file(destinationperm, copyperm, destinationfile, newpath, destinationhash, head);
		            write(fd, &check_err, sizeof(int));
		            // if the check_err has an error stop this loop
		            if(check_err == MATCH_ERROR){
		                continue;
		            }
		            // if mismatch read the file contents and copy it over
		            else if(check_err == MISMATCH){
		                FILE *fp;
		                char buff = '\0';
		                if((fp = fopen(newpath, "wb")) == NULL){
		                    fprintf(stderr, "insufficient permission for the destpath file \'%s\' \n", newpath);
		                    check_err = MATCH_ERROR;
		                    write(fd, &check_err, sizeof(int));
		                    continue;
		                }
		                read(fd, &fsize, sizeof(int));

		                for(int i = 0; i < fsize; i++){ 
		                    if(read(fd, &buff, 1) < 0){      
		                        trans = TRANSMIT_ERROR;  
		                        continue;  
		                    }
		                    if(fwrite(&buff, 1, 1, fp) < 0){
		                        trans = TRANSMIT_ERROR;
		                        continue;
		                    }
		                }
		                write(fd, &trans, sizeof(int));
		                fclose(fp);
		            }   
		        }

		        // if it's a directory create it
		        else if (S_ISDIR(head->mode)){
		            check_err = make_dir(destinationperm, copyperm, destinationfile, newpath);
		            write(fd, &check_err, sizeof(int));
		        }
		    }
    	}      
    close(fd);
    }
}

int send_server(char *source, char *destination, int sockt){
    struct fileinfo *head = malloc(sizeof(struct fileinfo));
    struct dirent *entry;
    struct stat file;
    char absolute[PATH_MAX];
    
    // check if entered path is valid
    if (lstat(source, &file) == -1){
        fprintf(stderr, "lstat: the name \'%s\' is not a file or directory\n", source);
        exit(1);
    }


    // get and copy absolute into path
    realpath(source, absolute);
    strcpy(head->path, absolute);
    head->mode = file.st_mode;

    mode_t mode;
    mode = htonl(head->mode);
    head->hash[0] = '\0';
    head->size = file.st_size;

    size_t size;
    size = htonl(head->size);
    // if it's a file
    if(S_ISREG(file.st_mode)){
        // open the file
        FILE* fp;
        // check file permissions
        if((fp = fopen(source, "rb")) == NULL){
            fprintf(stderr, "insufficient permission for the file \'%s\' \n", source);
            exit(1);
        }
        strncpy(head->hash, hash(fp), HASH_SIZE);
        fclose(fp);
    }
    // server writes
    write(sockt, destination, MAXPATH*sizeof(char *));
    write(sockt, head->path, MAXPATH*sizeof(char));
    write(sockt, &mode, sizeof(mode_t));
    write(sockt, head->hash, HASH_SIZE*sizeof(char));
    write(sockt, &size, sizeof(size_t));

    // checks the returned matching
    int check_match;
    read(sockt, &check_match, sizeof(int));

    // check_match returned an error
    if(check_match == MATCH_ERROR){
        fprintf(stderr, "match error with directory: %s\n", source);
        exit(1);
    }
    // in the case of mismatch, transfer files to server
    if(check_match == MISMATCH){
        FILE* fp;
        int fsize;
        if((fp = fopen(source, "rb")) == NULL){
            fprintf(stderr, "insufficient permission for the file \'%s\' \n", source);
            exit(1);
        }
        char *buff;

        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        buff = malloc((fsize + 1)*sizeof(char *));

        write(sockt, &fsize, sizeof(int));
        // loop through file
        for(int i = 0; i < fsize; i++) {
            if(fread((buff + i), 1, 1, fp) < 0){
                perror("mate: fread");
                exit(1);
            }
            if(write(sockt, (buff + i), 1) < 0){
                perror("mate: write");
                exit(1);
            }
        }
        int trans;
        read(sockt, &trans, sizeof(int));

        // server transmit error
        if(trans == TRANSMIT_ERROR){
            fprintf(stderr, "server transmit error with file: \'%s\'", source);
        }
        free(buff);
        fclose(fp);
    }
    // if path is directory 
    if (S_ISDIR(file.st_mode)){
        char path[MAXPATH], newpath[MAXPATH];
        DIR *dir;
        const char *name;
        // get the name
        name = source;
	    if(strrchr(source, '/') != NULL){
	        name = strrchr(source, '/');
	        name = name + 1;
	    }
        // put the dir into destination
        snprintf(newpath, sizeof(newpath)-1, "%s/%s", destination, name);
        // exit if no permissions
        if(!(dir = opendir(source))){
                fprintf(stderr, "insufficient permission for the directory \'%s\' \n", source);
                exit(1);
        }
        // loop while there is still a subdirectory
        while((entry = readdir(dir)) != NULL){
            // get rid of subdirs with a '.' and get path to new subdirs
            if(entry->d_name[0] == '.' || S_ISLNK(file.st_mode)){
                continue;
            }
            snprintf(path, sizeof(path)-1, "%s/%s", source, entry->d_name);
            send_server(path, newpath, sockt);
        }
        closedir(dir);
    }
    free(head);
    return 0;
}

//listener setup
int setup(int port){
    int high = 1, status;
    int listener;
    struct sockaddr_in self;
    if ((listener = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      	perror("socket");
      	exit(1);
    }

    // this affirms that we may use port after termination
    status = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                        (const char *) &high, sizeof(high));
    if(status == -1) {
      	perror("setsockopt -- REUSEADDR");
    }

    self.sin_family = AF_INET; 
    self.sin_addr.s_addr = INADDR_ANY;
    self.sin_port = htons(PORT); 
    memset(&self.sin_zero, 0, sizeof(self.sin_zero)); 

    printf("Listening high %d\n", PORT);
 
    if (bind(listener, (struct sockaddr *)&self, sizeof(self)) == -1) {
      	perror("bind"); // probably means port is in use
    	exit(1);
    }

    if (listen(listener, 5) == -1) {
      	perror("listen"); 
      	exit(1);
    }
    return listener;  
}

// creates directory
int make_dir(int destinationperm, int copyperm, struct stat destinationfile, char *newpath){
    mode_t p_msk = umask(0);
    // simply createes directory with error checking
    if(mkdir(newpath, copyperm)== -1){
        if (lstat(newpath, &destinationfile) == -1){
            fprintf(stderr, "lstat: the name \'%s\' is not a file or directory\n", newpath);
            return MATCH_ERROR;
        }
        // if it is nto a directory
        if(!(S_ISDIR(destinationfile.st_mode))){ 
            fprintf(stderr, "file name already exits: \'%s\' \n", newpath);
            return MATCH_ERROR; 
        } 
        // if it is a directory
        else{
        	destinationperm = destinationfile.st_mode & PERM_VAL; 
            if(copyperm != destinationperm){
                if(chmod(newpath, copyperm) == -1){
                    fprintf(stderr, "chmod error for file \'%s\' \n", newpath);
                    return MATCH_ERROR;
                } 
            }
        }
    }
    umask(p_msk);
    return MATCH;
}

int make_file(int destinationperm, int copyperm, struct stat destinationfile, char *newpath, char destinationhash[HASH_SIZE], struct fileinfo *head){
    FILE* fp;
    // make the file/open the file for reading
    if((fp = fopen(newpath, "ab+")) == NULL){
        if (lstat(newpath, &destinationfile) == -1){
            fprintf(stderr, "lstat: the name \'%s\' is not a file or directory\n", newpath);
            return MATCH_ERROR;
        }
        // case 1: file replacing file
        if(S_ISDIR(destinationfile.st_mode)){
            fprintf(stderr, "directory name already exists: %s \n", newpath);
            return MATCH_ERROR;
        }
        // case 2: file with no perm
        else{
            fprintf(stderr, "insufficient permission for the destpath file \'%s\' \n", newpath);
            return MATCH_ERROR;
        }
    }

    // error messages
    if (lstat(newpath, &destinationfile) == -1){
        fprintf(stderr, "lstat: the name \'%s\' is not a file or directory\n", newpath);
        return MATCH_ERROR;
    }
    // set same permissions for new file
    destinationperm = destinationfile.st_mode & PERM_VAL;
    if(copyperm != destinationperm){
        if(chmod(newpath, copyperm) == -1){
            fprintf(stderr, "chmod error for file \'%s\' \n", newpath);
            return MATCH_ERROR;
        }
    }

    // if the two files have different sizes
    if((head->size == destinationfile.st_size)){
        strcpy(destinationhash, hash(fp));
        if(strcmp(head->hash, destinationhash)){
            return MISMATCH;
        }
    }
    else{
        return MISMATCH;
    }

    fclose(fp);
    return MATCH;
}
