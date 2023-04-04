# TODO: Add test files
CC = g++
TARGET = braun-a4
OUTPUT = $(TARGET)
TARFILE = $(TARGET).tar.gz

all: a4w23 tar

tar: $(TARFILE)

$(TARFILE): a4w23
	tar -czvf $(TARFILE) a4w23.cc Makefile README.md

a4w23: a4w23.cc
	$(CC) -o a4w23 a4w23.cc

test1: test1.cc
	$(CC) -o test1 test1.cc

clean:
	rm -f a4w23 *.tar.gz
