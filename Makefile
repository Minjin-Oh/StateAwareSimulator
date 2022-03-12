CC = gcc
CFLAGS = -g -Wall 
TARGET = statesimul.out
OBJS = init.o simul.o main.o misc.o list.o hot.o util.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) -lm

init.o : init.c
	$(CC) -c -o init.o init.c

simul.o : simul.c
	$(CC) -c -o simul.o simul.c

main.o : main.c
	$(CC) -c -o main.o main.c

misc.o : misc.c
	$(CC) -c -o misc.o misc.c

list.o : list.c
	$(CC) -c -o list.o list.c

hot.o : hot.c
	$(CC) -c -o hot.o hot.c

util.o : util.c
	$(CC) -c -o util.o util.c
	
clean:
	rm -f *.o
	rm $(OBJECT) $(TARGET)
