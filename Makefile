CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -pthread -lm

SRCS = myChannels.c parse.c lock.c
OBJS = $(SRCS:.c=.o)

all: myChannels

myChannels: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) myChannels
