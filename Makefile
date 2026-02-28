CXX = g++
CXXFLAGS = -std=c++17 -Wall -Iinclude
LDFLAGS = -lncurses

SRC = src/main.cpp
OUT = build/editor

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

run:
	./$(OUT)

clean:
	rm -f $(OUT)
