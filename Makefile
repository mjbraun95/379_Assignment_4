# TODO: Add test files
CC = g++
TARGET = braun-a4
OUTPUT = $(TARGET)
TARFILE = $(TARGET).tar.gz

all: a4w23 tar

tar: $(TARFILE)

$(TARFILE): a4w23
	tar -czvf $(TARFILE) a4w23.cpp Makefile README.md

a4w23: a4w23.cpp
	$(CC) -o a4w23 a4w23.cpp

test1: test1.cpp
	$(CC) -o test1 test1.cpp

freerange: freerange.cpp
	$(CC) -o freerange freerange.cpp

clean:
	rm -f freerange test1 a4w23 *.tar.gz
	# rm -f a4w23 *.tar.gz
