# udpCode makefile
# written by Hugh Smith - Feb 2021

CC = gcc
CFLAGS = -g -Wall


SRC =  networks.c gethostbyname.c safeUtil.c pdu.c checksum.c
OBJS = networks.o gethostbyname.o safeUtil.o pdu.o checksum.o

#uncomment next two lines if your using sendtoErr() library
LIBS += libcpe464.2.21.a -lstdc++ -ldl
CFLAGS += -D__LIBCPE464_

all:  rcopy server

windowTest: windowTest.c pdu.o window.o checksum.o
	$(CC) $(CFLAGS) -o windowTest windowTest.c pdu.o window.o checksum.o

rcopy: rcopy.c $(OBJS) 
	$(CC) $(CFLAGS) -o rcopy rcopy.c $(OBJS) $(LIBS)

server: server.c $(OBJS) 
	$(CC) $(CFLAGS) -o server server.c  $(OBJS) $(LIBS)

checksum.o:
	ar xv libcpe464.2.21.a

%.o: %.c *.h 
	gcc -c $(CFLAGS) $< -o $@ 

cleano:
	rm -f *.o

clean:
	rm -f server rcopy windowTest *.o

