CC = g++
TARGET = braun-a4
TARFILE = $(TARGET).tar.gz

all: a4w23 tar

tar: $(TARFILE)

$(TARFILE): a4w23
	tar -czvf $(TARFILE) a4w23.cpp Makefile README.md input_file1.txt input_file2.txt

a4w23: a4w23.cpp
	$(CC) -o a4w23 a4w23.cpp

clean:
	rm -f a4w23 *.tar.gz