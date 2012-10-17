# This is Makefile for OFS(Organizing File System).

APPLICATION = ofs
CC = gcc
CFLAGS = -Wall -DFUSE_USE_VERSION=26 -D_FILE_OFFSET_BITS=64
OBJS = ofs.o node.o lib.o

RM = rm -rf


$(APPLICATION) : $(OBJS)
	$(CC) $(CFLAGS) -o $(APPLICATION) $(OBJS) -lfuse

ofs.o : ofs.c
	$(CC) $(CFLAGS) -c $^ -lfuse

node.o : node.c
	$(CC) $(CFLAGS) -c $^ -lfuse

lib.o : lib.c
	$(CC) $(CFLAGS) -c $^ -lfues

clean :
	$(RM) $(OBJS)
	$(RM) $(APPLICATION)
