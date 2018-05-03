# A simple Makefile, to build run: make all 

CC	= gcc
#compiler flags here
CFLAGS = -O3 -Wall -Wextra

#linker flags here
LFLAGS = -Wall

SOURCES	:= $(wildcard /*.c)
INCLUDES := $(wildcard /*.h))
OBJECTS	:= last_print.o utmp_print.o fail_print.o acct_print.o


.PHONY: all clean remove
all: utmp_print last_print fail_print acct_print

utmp_print: $(OBJECTS)
	@$(CC) -o $@ $(LFLAGS) utmp_print.o

last_print: $(OBJECTS)
	@$(CC) -o $@ $(LFLAGS) last_print.o 

fail_print: $(OBJECTS)
	@$(CC) -o $@ $(LFLAGS) fail_print.o

acct_print: $(OBJECTS)
	@$(CC) -o $@ $(LFLAGS) acct_print.o

$(OBJECTS): ./%.o : ./%.c
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@$ rm -f $(OBJECTS)

remove: clean
	@$ rm -f last_print utmp_print fail_print acct_print
