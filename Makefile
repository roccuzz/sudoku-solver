CC = gcc
CFLAGS = -Wall -Wextra -g

TARGET = sudoku_solver

SRC = sudoku_solver.c

OBJ = $(SRC:.c=.o)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

clean:
	rm -f $(OBJ) $(TARGET)

