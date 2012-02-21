CXXFLAGS=-std=c++0x -Wall -Wextra
OBJ=tiffraw.o
LIBS=-ltiff

all: $(OBJ) tifftoraw

tifftoraw: tiffraw.o
	$(CXX) $^ -o $@ $(LIBS)

clean:
	rm -f $(OBJ)
	rm -f tifftoraw
