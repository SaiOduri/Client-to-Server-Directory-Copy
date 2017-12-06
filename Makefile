PORT=51842
FLAGS = -DPORT=$(PORT) -g -Wall -std=gnu99
DEPENDENCIES = hash.h ftree.h
REQ = ftree.o hash_functions.o

all: rcopy_client rcopy_server

rcopy_client: ${REQ} rcopy_client.o
	gcc ${FLAGS} -o $@ $^

rcopy_server: ${REQ} rcopy_server.o
	gcc ${FLAGS} -o $@ $^

%.o: %.c ${DEPENDENCIES}
	gcc ${FLAGS} -c $<

clean:
	rm *.o rcopy_client rcopy_server
