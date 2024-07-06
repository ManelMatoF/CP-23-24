CC=gcc
CFLAGS=-Wall -pthread -g
LIBS=
OBJS=sum.o options.o

PROGS= sum

all: $(PROGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $<

sum: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -f $(PROGS) *.o *~

