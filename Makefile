CC = gcc
CFLAGS = -g -Wall 
TARGET = statesimul.out
OBJS =  init.o parse.o misc.o util.o \
		list.o IOlist.o \
		gen_task.o  IOgen.o \
		findRR.o findGC.o \
	   	IOsimul.o IOsim_q.o rrsim_q.o emul.o hot.o logger.o \
		job_main.o

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) -lm -ldl -lglpk

clean:
	rm -f *.o
	rm $(OBJECT) $(TARGET)
