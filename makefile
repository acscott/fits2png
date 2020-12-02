.POSIX:
IDIR=../include
CC=gcc
CFLAGS=-Wall -O -g
LDLIBS= -lm -lcfitsio -lpng -lm


all: fits2png

fits2png: fits2png.o
	$(CC) $(LDFLAGS) -o fits2png fits2png.o $(LDLIBS)

fits2png.o: fits2png.c

clean:
	rm -f fits2png fist2png.o

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -I$(IDIR) -c $< 
