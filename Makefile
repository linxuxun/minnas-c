CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I./src -g -O2
LDFLAGS = -lm

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
BIN = minnas

.PHONY: all clean test run-tests

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(BIN)
	@echo "No test runner yet - use: gcc tests/*.c src/*.c -o test_runner -lm"
	@./$(BIN) init --path /tmp/minnas_test && ./$(BIN) status

clean:
	rm -rf $(OBJ) $(BIN) .minnas/

run-tests: $(BIN)
	./$(BIN) init --path /tmp/minnas_test
	./$(BIN) fs write /hello.txt "Hello from MiniNAS!"
	./$(BIN) commit "initial"
	./$(BIN) log
	./$(BIN) stats
	./$(BIN) gc
	rm -rf /tmp/minnas_test
	echo "All tests passed!"
