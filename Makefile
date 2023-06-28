CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -pthread -lm

SRCS = myChannels.c parse.c
OBJS = $(SRCS:.c=.o)

all: myChannel

myChannel: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) myChannel
