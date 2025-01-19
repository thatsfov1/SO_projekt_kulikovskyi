CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I.

# Executable names
EXECUTABLES = main klient kasjer piekarz kierownik

# Source files
SRCS = main.c klient.c kasjer.c piekarz.c kierownik.c funkcje.c

# Object files
OBJS = $(SRCS:.c=.o)

# Phony targets
.PHONY: all clean

# Default target
all: $(EXECUTABLES)

# Compile and link each executable
main: main.o funkcje.o
    $(CC) $(CFLAGS) -o $@ $^

klient: klient.o funkcje.o
    $(CC) $(CFLAGS) -o $@ $^

kasjer: kasjer.o funkcje.o
    $(CC) $(CFLAGS) -o $@ $^

piekarz: piekarz.o funkcje.o
    $(CC) $(CFLAGS) -o $@ $^

kierownik: kierownik.o funkcje.o
    $(CC) $(CFLAGS) -o $@ $^

# Compile object files
%.o: %.c struktury.h funkcje.h
    $(CC) $(CFLAGS) -c $< -o $@

# Clean up
clean:
    rm -f $(EXECUTABLES) *.o