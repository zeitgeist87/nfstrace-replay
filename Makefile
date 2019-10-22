CC      = g++
CFLAGS  = -flto -Wall -Wextra -Wpedantic -O2 -rdynamic -std=c++14 -D_FILE_OFFSET_BITS=64
LDFLAGS = -flto -lncurses -D_FILE_OFFSET_BITS=64

SRC = ConsoleDisplay.cpp nfsreplay.cpp TransactionMgr.cpp FileSystemTree.cpp Parser.cpp TreeNode.cpp
OBJ = $(SRC:%.cpp=%.o)

all: nfsreplay

nfsreplay: $(OBJ)
	$(CC) $(CFLAGS) -o nfsreplay $(OBJ) $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm nfsreplay *.o -f
