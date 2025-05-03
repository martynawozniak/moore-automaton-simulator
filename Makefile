CC       = gcc
CFLAGS   = -Wall -Wextra -Wno-implicit-fallthrough -std=gnu17 -fPIC -O2
LDFLAGS  = -shared \
           -Wl,--wrap=malloc \
           -Wl,--wrap=calloc \
           -Wl,--wrap=realloc \
           -Wl,--wrap=reallocarray \
           -Wl,--wrap=free \
           -Wl,--wrap=strdup \
           -Wl,--wrap=strndup

SRCS     = ma.c memory_tests.c
OBJS     = $(SRCS:.c=.o)
HEADERS  = ma.h memory_tests.h

TARGET   = libma.so

.PHONY: all clean run_example

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

ma.o: ma.c ma.h
	$(CC) $(CFLAGS) -c $< -o $@

memory_tests.o: memory_tests.c memory_tests.h
	$(CC) $(CFLAGS) -c $< -o $@

ma_example.o: ma_example.c ma.h
	$(CC) $(CFLAGS) -c $< -o $@

ma_example: ma_example.o $(TARGET)
	$(CC) $(CFLAGS) -L. -o $@ $< -lma

run_example: ma_example
	./ma_example one

clean:
	rm -f $(OBJS) $(TARGET) ma_example ma_example.o