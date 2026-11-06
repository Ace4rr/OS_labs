CC=clang
CFLAGS= -Wall -Wextra -Werror -O2 -std=c11 -pthread
LDFLAGS= -pthread

TARGET=lab3
SRCS=main.c determinant.c
OBJS=$(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c determinant.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
