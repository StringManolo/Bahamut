CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20 -I./include -I./tests
TEST_FLAGS = -lgtest -lgtest_main -pthread -lstdc++fs

TARGET = bin/bahamut
TEST_TARGET = bin/run_tests

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

# Regla para tests - enlazar sin main de core.cpp
test: $(CORE_OBJ) $(TEST_OBJ)
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o $(TEST_TARGET) $(CORE_OBJ) $(TEST_OBJ) $(TEST_FLAGS)
	./$(TEST_TARGET)

# Regla para tests con cobertura
test-coverage: CXXFLAGS += --coverage
test-coverage: test
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory coverage_report

# Regla para tests de integración
test-integration: $(CORE_OBJ) tests/integration_test.o
	mkdir -p bin
	$(CXX) $(CXXFLAGS) -o bin/integration_tests $(CORE_OBJ) tests/integration_test.o $(TEST_FLAGS)
	./bin/integration_tests

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(TARGET) $(TEST_TARGET) bin/integration_tests
	rm -rf coverage_report coverage.info *.gcno *.gcda

.PHONY: all clean test test-coverage test-integration
