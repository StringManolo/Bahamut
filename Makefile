CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20 -I./include -I./tests
TEST_FLAGS = -lgtest -lgtest_main -pthread -lstdc++fs

TARGET = bin/bahamut
TEST_TARGET = bin/run_tests
SINGLE_TARGET = bin/single_test

SRC = cli/cli.cpp core/core.cpp
CORE_SRC = core/core.cpp
TEST_SRC = $(wildcard tests/*.cpp)

OBJ = $(SRC:.cpp=.o)
CORE_OBJ = core/core.o
TEST_OBJ = $(TEST_SRC:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

test: $(CORE_OBJ) $(TEST_OBJ)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $(TEST_TARGET) $(CORE_OBJ) $(TEST_OBJ) $(TEST_FLAGS)
	./$(TEST_TARGET)

test-single: $(CORE_OBJ)
	@if [ -z "$(FILE)" ]; then echo "Error: Uso -> make test-single FILE=tests/nombre_del_test.cpp"; exit 1; fi
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -c $(FILE) -o single_test.o
	$(CXX) $(CXXFLAGS) -o $(SINGLE_TARGET) $(CORE_OBJ) single_test.o $(TEST_FLAGS)
	./$(SINGLE_TARGET)
	rm -f single_test.o *.gcno

test-coverage: CXXFLAGS += --coverage
test-coverage: test
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory coverage_report

test-integration: $(CORE_OBJ) tests/integration_test.o
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o bin/integration_tests $(CORE_OBJ) tests/integration_test.o $(TEST_FLAGS)
	./bin/integration_tests

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(TARGET) $(TEST_TARGET) $(SINGLE_TARGET) bin/integration_tests
	rm -rf coverage_report coverage.info *.gcno *.gcda

.PHONY: all clean test test-coverage test-integration test-single
