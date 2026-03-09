/*
 * sudoku_solver.c - Sudoku solver via recursive backtracking
 *
 * Usage: ./sudoku <file>
 * The file must contain exactly 81 digits (0 = empty cell).
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Costants */

#define GRID_SIZE   9
#define CELL_COUNT  81
#define EMPTY_CELL  0
#define MIN_VAL     1
#define MAX_VAL     9

/* Types */

typedef struct
{
	int cell[9];
} row;

typedef struct
{
	row rows[9];
} grid;

typedef enum {
    FALSE = 0,
    TRUE  = 1
} bool;

/* Forward declaration */

static int   char_to_digit(char c);
static int  *read_from_file(const char *filepath);
static grid *create_grid(void);
static void  insert_grid(grid *g, const int *vals);
static bool  check_row(const row *row, int val);
static bool  check_col(const grid *g, int col, int val);
static bool  check_subgrid(const grid *g, int row, int col, int val);
static bool  is_valid_placement(const grid *g, int row, int col, int val);
static bool  is_full(const grid *g);
static bool  solve_sudoku(grid *g);
static void  print_grid(const grid *g);
static void  free_resources(int *data, grid *g);


/* Implementation */

/**
 * Converts an ASCII character to its integer digit value.
 * @param c  Character to convert.
 * @return   Value 0-9 if it is a digit, -1 otherwise.
 */
static int char_to_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    return -1;
}

/**
 * Reads exactly CELL_COUNT digits from a text file.
 * All non-digit characters (spaces, newlines, separators...) are ignored.
 *
 * @param filepath  Path to the file to read.
 * @return          Dynamically allocated array of CELL_COUNT integers.
 *                  The caller is responsible for calling free().
 */
static int *read_from_file(const char *filepath)
{
  int  fd;
  int  n;
  int  count = 0;
  char buf[BUFSIZ];

  int *values = malloc(CELL_COUNT * sizeof(int));
  if (!values) {
      perror("malloc");
      exit(EXIT_FAILURE);
  }

  if ((fd = open(filepath, O_RDONLY, 0664)) == -1) {
    perror("open");
    free(values);
    exit(EXIT_FAILURE);
  }

  /* Read in chunks, collecting only digit characters */
  while ((n = read(fd, buf, BUFSIZ)) > 0 && count < CELL_COUNT) {
    for (int i = 0; i < n && count < CELL_COUNT; i++) {
        int digit = char_to_digit(buf[i]);
        if (digit != -1) values[count++] = digit;
      }
  }

  if (n == -1) {
    perror("read");
    free(values);
    close(fd);
    exit(EXIT_FAILURE);
  }

  if (close(fd) == -1) {
    perror("close");
    free(values);
    exit(EXIT_FAILURE);
  }

   /* Ensure the file contained exactly 81 digits */
  if (count != CELL_COUNT) {
    fprintf(stderr, "Error: file must contain exactly %d digits "
        "(found: %d).\n", CELL_COUNT, count);
    free(values);
    exit(EXIT_FAILURE);
  }

  return values;
}

/**
 * Allocates and initializes an empty grid (all cells set to EMPTY_CELL).
 *
 * @return  Pointer to the allocated grid (caller must free()).
 */
static grid *create_grid(void)
{
  grid *g = malloc(sizeof(grid));
  if (!g) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  for (int row = 0; row < GRID_SIZE; row++)
    for (int col = 0; col < GRID_SIZE; col++)
      g->rows[row].cell[col] = EMPTY_CELL;

  return g;
}

/**
 * Fills the grid with the provided values, row by row.
 *
 * @param g     grid to populate.
 * @param vals  Array of CELL_COUNT integers (0 = empty cell).
 */
static void insert_grid(grid *g, const int *vals)
{
  int k = 0;
  for (int row = 0; row < GRID_SIZE; row++)
    for (int col = 0; col < GRID_SIZE; col++)
      g->rows[row].cell[col] = vals[k++];
}

/**
 * Checks whether `val` is already present in the given row.
 *
 * @return TRUE if the value is absent (placement is valid), FALSE otherwise.
 */
static bool check_row(const row *row, int val)
{
  for (int col = 0; col < GRID_SIZE; col++)
    if (row->cell[col] == val) return FALSE;
  return TRUE;
}

/**
 * Checks whether `val` is already present in column `col`.
 *
 * @return TRUE if the value is absent (placement is valid), FALSE otherwise.
 */
static bool check_col(const grid *g, int col, int val)
{
  for (int row = 0; row < GRID_SIZE; row++)
    if (g->rows[row].cell[col] == val) return FALSE;
  return TRUE;
}

/**
 * Checks whether `val` is already present in the 3x3 subgrid
 * that contains cell (row, col).
 *
 * @return TRUE if the value is absent (placement is valid), FALSE otherwise.
 */
static bool check_subgrid(const grid *g, int row, int col, int val)
{
  int start_row = row - (row % 3);
  int start_col = col - (col % 3);

  for (int r = start_row; r < start_row + 3; r++)
    for (int c = start_col; c < start_col + 3; c++)
      if (g->rows[r].cell[c] == val) return FALSE;

  return TRUE;
}

/**
 * Returns TRUE if placing `val` at (row, col) is legal,
 * i.e. it passes all three checks: row, column, and subgrid.
 */
static bool is_valid_placement(const grid *g, int row, int col, int val)
{
  return check_row(&g->rows[row], val)
    && check_col(g, col, val)
      && check_subgrid(g, row, col, val);
}

/**
 * Returns TRUE if every cell in the grid has a non-zero value.
 */
static bool is_full(const grid *g)
{
  for (int row = 0; row < GRID_SIZE; row++)
    for (int col = 0; col < GRID_SIZE; col++)
      if (g->rows[row].cell[col] == EMPTY_CELL) return FALSE;
  return TRUE;
}

/**
 * Solves the sudoku in-place using recursive backtracking:
 *  1. Find the first empty cell.
 *  2. Try digits 1-9; if valid, place the digit and recurse.
 *  3. If recursion fails, undo the placement (backtrack) and try the next digit.
 *
 * @return TRUE if the puzzle was solved, FALSE if no solution exists.
 */
static bool solve_sudoku(grid *g)
{
  /* Base case: grid is full → solution found */
  if (is_full(g))
    return TRUE;

  /* Find the first empty cell */
  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      if (g->rows[row].cell[col] != EMPTY_CELL) continue;

      /* Try every candidate digit */
      for (int val = MIN_VAL; val <= MAX_VAL; val++) {
        if (!is_valid_placement(g, row, col, val)) continue;

        g->rows[row].cell[col] = val;

        if (solve_sudoku(g)) return TRUE;

        /* Backtrack: this digit did not lead to a solution */
        g->rows[row].cell[col] = EMPTY_CELL;
      }

      /* No digit worked → dead end, propagate failure */
      return FALSE;
    }
  }

  return FALSE;
}

/**
 * Prints the grid to stdout in a human-readable format with separators.
 * The parameter is const because the function does not modify the grid.
 */
static void print_grid(const grid *g)
{
  printf("-------------------------\n");
  for (int row = 0; row < GRID_SIZE; row++) {
    for (int col = 0; col < GRID_SIZE; col++) {
      if (col % 3 == 0) printf("| ");
      printf("%d ", g->rows[row].cell[col]);
    }
    printf("|\n");
    if (row % 3 == 2) printf("-------------------------\n");
  }
}

/**
 * Frees all dynamically allocated resources.
 * Passing NULL is safe (free(NULL) is a no-op per the C standard).
 */
static void free_resources(int *data, grid *g)
{
  free(data);
  free(g);
}

/* Entry Point */

int main(int argc, char **argv)
{
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  /* Load values from the input file */
  int *values = read_from_file(argv[1]);

  /* Build and populate the grid */
  grid *g = create_grid();
  insert_grid(g, values);

  /* Display the initial grid */
  printf("=== Initial grid ===\n");
  print_grid(g);

  /* Solve and display the result */
  if (solve_sudoku(g)) {
    printf("\n=== Solution found ===\n");
    print_grid(g);
  } else {
    printf("\nNo solution found for this puzzle.\n");
  }

  free_resources(values, g);
  return EXIT_SUCCESS;
}

