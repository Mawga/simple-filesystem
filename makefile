CC = g++
ARGS = -std=c++11 -Wall

all: fs

fs: fs.cc
	$(CC) $(ARGS) -c -o fs.o fs.cc

clean:
	rm -rf *.o *~

