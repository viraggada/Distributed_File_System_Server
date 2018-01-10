CC=gcc
CFLAGS=-g -O0 -pthread
#-Wall -Werror
LFLAGS=-lssl -lcrypto#-Wl -Map=main.map

SSRCS=server.c
CSRCS=client.c
PROGRAMS=dfs dfc

#Default make command builds executable file
all: $(PROGRAMS)

dfs: $(SSRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

dfc: $(CSRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

.PHONY: clean

clean:
	rm -rf $(PROGRAMS) final_list list
