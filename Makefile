CC = gcc
CFLAGS = -g -W 
LDFLAGS = -lm -ldl

#source directory
SRC_DIR = ./src

#obj file directory
OBJ_DIR = ./obj

TARGET = statesimul.out

#specify sources and objs
SRCS = $(notdir $(wildcard $(SRC_DIR)/*.c))

OBJS = $(SRCS:.c=.o)

OBJECTS = $(patsubst %.o, $(OBJ_DIR)/%.o,$(OBJS))
DEPS = $(OBJECTS:.o=.d)

#OBJS =  init.o parse.o misc.o util.o \
		list.o IOlist.o \
		gen_task.o  IOgen.o \
		findRR.o findGC.o assignW.o findW.o\
	   	IOsimul.o IOsim_q.o rrsim_q.o emul.o hot.o \
		emul_logger.o logger.o \
		emul_main.o

all : $(TARGET)

$(OBJ_DIR)/%.o : $(SRC_DIR)/%.c 
	$(CC) $(CFLAGS) -c $< -o $@ -MD $(LDFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS)



clean:
	rm $(OBJECTS) $(DEPS) $(TARGET)
