# Makefile:
#
#	Make Visi-Genie Demos on the Raspberry Pi
#
#	Gordon Henderson, December 2012
###############################################################################

#DEBUG	= -g -O0
DEBUG	= -O2
CC	= $(GCC)
INCLUDE	= -I/usr/local/include
CFLAGS	= $(DEBUG) -Wall $(INCLUDE) -Winline -pipe
LDFLAGS = -L/usr/local/lib
LIBS    = -lpthread -lrt -lm 

SRC	= cnc.c gcode.c motor.c keyboard.c geometry.c socket.c unitTest.c

# May not need to  alter anything below this line
###############################################################################

OBJ	=	$(SRC:.c=.o)
BINS	=	$(SRC:.c=)

cnc:	$(OBJ)
	@echo [link]
	@$(CC) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

.c.o:
	@echo [Compile] $<
	@$(CC) -c $(CFLAGS) $< -o $@

.PHONEY:	clean
clean:
	rm -f $(OBJ) $(BINS) *~ core tags *.bak cnc

.PHONEY:	tags
tags:	$(SRC)
	@echo [ctags]
	@ctags $(SRC)

.PHONEY:	depend
depend:
	makedepend -Y $(SRC)

# DO NOT DELETE
