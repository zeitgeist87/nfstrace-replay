CC      = g++
CFLAGS  = -Wall -O2 -std=c++11
LDFLAGS = -lncurses

OBJ = cnfsparse.o gc.o nfstree.o operations.o parser.o

cnfsparse: $(OBJ)
	$(CC) $(CFLAGS) -o cnfsparse $(OBJ) $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm cnfsparse *.o -f
