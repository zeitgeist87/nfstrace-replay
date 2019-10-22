CC      = g++
CFLAGS  = -flto -march=native -fno-fat-lto-objects -Wall -Wextra -Wpedantic -O3 -rdynamic -std=c++14 -D_FILE_OFFSET_BITS=64
LDFLAGS = -flto -march=native -fwhole-program -fno-fat-lto-objects -lncurses -D_FILE_OFFSET_BITS=64

SRC = ConsoleDisplay.cpp nfsreplay.cpp TransactionMgr.cpp FileSystemTree.cpp Parser.cpp TreeNode.cpp
OBJ = $(SRC:%.cpp=%.o)

all: nfsreplay

nfsreplay: $(OBJ)
	$(CC) $(CFLAGS) -o nfsreplay $(OBJ) $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm nfsreplay *.o -f
