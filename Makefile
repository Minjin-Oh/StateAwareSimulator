CC = gcc
CFLAGS = -g -Wall 
TARGET = statesimul.out
OBJS = init.o IOsimul.o simul.o job_main.o misc.o list.o hot.o findRR.o gen_task.o util.o IOgen.o lpsolve.o logger.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) -lm -ldl -lglpk

clean:
	rm -f *.o
	rm $(OBJECT) $(TARGET)
