CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20 -I./include

TARGET = bin/bahamut
SRC = cli/cli.cpp core/core.cpp
OBJ = $(SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
