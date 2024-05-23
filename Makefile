CC = gcc
CFLAGS = -Wall -I/usr/include/protobuf-c
LDFLAGS = -lprotobuf-c

all: servidor cliente

servidor: servidor.o chat.pb-c.o
	$(CC) -o servidor servidor.o chat.pb-c.o $(LDFLAGS)

cliente: cliente.o chat.pb-c.o
	$(CC) -o cliente cliente.o chat.pb-c.o $(LDFLAGS)

servidor.o: servidor.c chat.pb-c.h
	$(CC) $(CFLAGS) -c servidor.c

cliente.o: cliente.c chat.pb-c.h
	$(CC) $(CFLAGS) -c cliente.c

chat.pb-c.o: chat.pb-c.c chat.pb-c.h
	$(CC) $(CFLAGS) -c chat.pb-c.c

clean:
	rm -f *.o servidor cliente
