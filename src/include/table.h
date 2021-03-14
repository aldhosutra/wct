#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO jadiin function punya return (-1 -> kalau gagal, 0 -> kalau berhasil)

#define TABLE_INITIAL_ROW 50
#define TABLE_INITIAL_COL 32
#define SERIALIZE_DIR "../res/"
#define tab_name(name)  #name

typedef struct {
  int ready;
  int row;
  int row_cap;
  int *col;
  int col_cap;
  int **data;
} itab_t; // Integer Type Table

typedef struct {
  int ready;
  int row;
  int row_cap;
  int *col;
  int col_cap;
  double **data;
} dtab_t; // Double Type Table

int vecmax(int *array, int arraySize);
int vec_csvcolcount (char csvfile[], char *delimiter);
int vec_linecount(char filename[]);

// itab_t Header
void itab_init(itab_t *itab);
void itab_add_row(itab_t *itab);
void itab_append_col(itab_t *itab, int row, int value);
void itab_append_row(itab_t *itab, int value[], int value_lenght);
int itab_get(itab_t *itab, int row, int col);
void itab_set(itab_t *itab, int row, int col, int value);
void itab_expand_row(itab_t *itab);
void itab_expand_col(itab_t *itab, int row);
int itab_sum_row(itab_t *itab, int col);
int itab_sum_col(itab_t *itab, int row);
int itab_sum(itab_t *itab);
double itab_avg_row(itab_t *itab, int col);
double itab_avg_col(itab_t *itab, int row);
double itab_avg(itab_t *itab);
void itab_prepend_col(itab_t *itab, int row, int value);
void itab_prepend_row(itab_t *itab, int value[], int lenght);
void itab_insert_col(itab_t *itab, int index);
void itab_insert_row(itab_t *itab, int index);
void itab_pop_col(itab_t *itab);
void itab_pop_row(itab_t *itab);
void itab_delete_col(itab_t *itab, int col_index);
void itab_delete_row(itab_t *itab, int row_index);
void itab_delete_cell(itab_t *itab, int row, int cols);
void itab_wipeval(itab_t *itab, int value);
int itab_search_col(itab_t *itab, int col, int value);
int itab_search_row(itab_t *itab, int row, int value);
int itab_count(itab_t *itab, int value);
void itab_swap_row(itab_t *itab, int row_source, int row_dest);
void itab_sort_asc(itab_t *itab);
void itab_sort_desc(itab_t *itab);
void itab_slice_row(itab_t *itab, int *dest, int row, int startpoint, int endpoint);
void itab_filter(itab_t *itab, int filter_array[], int filter_lenght);
void itab_print(itab_t *itab);
void itab_compact(itab_t *itab);
void itab_copy(itab_t *from, itab_t *to);
void itab_read_csv(itab_t *itab, char *csvfile, char *delimiter);
void itab_serialize (itab_t *itab, char *name);
void itab_deserialize (itab_t *itab, char *name);
void itab_free(itab_t *itab);

// dtab_t Header
void dtab_init(dtab_t *dtab);
void dtab_add_row(dtab_t *dtab);
void dtab_append_col(dtab_t *dtab, int row, double value);
void dtab_append_row(dtab_t *dtab, double value[], int value_lenght);
double dtab_get(dtab_t *dtab, int row, int col);
void dtab_set(dtab_t *dtab, int row, int col, double value);
void dtab_expand_row(dtab_t *dtab);
void dtab_expand_col(dtab_t *dtab, int row);
double dtab_sum_row(dtab_t *dtab, int col);
double dtab_sum_col(dtab_t *dtab, int row);
double dtab_sum(dtab_t *dtab);
double dtab_avg_row(dtab_t *dtab, int col);
double dtab_avg_col(dtab_t *dtab, int row);
double dtab_avg(dtab_t *dtab);
void dtab_prepend_col(dtab_t *dtab, int row, double value);
void dtab_prepend_row(dtab_t *dtab, double value[], int lenght);
void dtab_insert_col(dtab_t *dtab, int index);
void dtab_insert_row(dtab_t *dtab, int index);
void dtab_pop_col(dtab_t *dtab);
void dtab_pop_row(dtab_t *dtab);
void dtab_delete_col(dtab_t *dtab, int col_index);
void dtab_delete_row(dtab_t *dtab, int row_index);
void dtab_delete_cell(dtab_t *dtab, int row, int cols);
void dtab_wipeval(dtab_t *dtab, double value);
int dtab_search_col(dtab_t *dtab, int col, double value);
int dtab_search_row(dtab_t *dtab, int row, double value);
int dtab_count(dtab_t *dtab, double value);
void dtab_swap_row(dtab_t *dtab, int row_source, int row_dest);
void dtab_sort_asc(dtab_t *dtab);
void dtab_sort_desc(dtab_t *dtab);
void dtab_slice_row(dtab_t *dtab, double *dest, int row, int startpoint, int endpoint);
void dtab_filter(dtab_t *dtab, int filter_array[], int filter_lenght);
void dtab_print(dtab_t *dtab);
void dtab_compact(dtab_t *dtab);
void dtab_copy(dtab_t *from, dtab_t *to);
void dtab_read_csv(dtab_t *dtab, char *csvfile, char *delimiter);
void dtab_serialize (dtab_t *dtab, char *name);
void dtab_deserialize (dtab_t *dtab, char *name);
void dtab_free(dtab_t *dtab);

int vecmax(int *array, int arraySize) {
    int ret = array[0];
    for (int i = 0; i < arraySize; i++) {if (array[i] > ret) ret = array[i];}
    return ret;
}

int vec_csvcolcount (char csvfile[], char *delimiter) {
  char line[2048];
  char* token = NULL;
  int colIndex = 0;
  FILE* fp = fopen(csvfile,"r");
  if (fp != NULL) {
    if (fgets(line, sizeof(line), fp) != NULL)
      for (token = strtok(line, delimiter); token != NULL; token = strtok(NULL, delimiter))
        colIndex++;
    fclose(fp);
    return colIndex;
  } else { return -1;}
}

int vec_linecount(char filename[]) {
  FILE *fp;
  int count = 1;
  char c;
  fp = fopen(filename, "r");
  if (fp == NULL) return -1;
  for (c = getc(fp); c != EOF; c = getc(fp))
    if (c == '\n')
      count = count + 1;
  fclose(fp);
  return count;
}

void itab_init(itab_t *itab) {
  if (itab->ready == 1) itab_free(itab);
  itab->ready = 1;
  itab->row = 0;
  itab->row_cap = TABLE_INITIAL_ROW;
  itab->col_cap = TABLE_INITIAL_COL;

  itab->col = calloc(itab->row_cap, sizeof(int) * itab->row_cap);
  itab->data = (int **)malloc(sizeof(int *) * itab->row_cap);
  for (int i = 0; i < itab->row_cap; i++)
    itab->data[i] = (int *)malloc(sizeof(int) * itab->col_cap);
}

void itab_add_row(itab_t *itab) {
  itab_expand_row(itab);
  itab->row++;
}

void itab_append_col(itab_t *itab, int row, int value) {
  itab_expand_col(itab, row);
  itab->data[row][itab->col[row]++] = value;
}

void itab_append_row(itab_t *itab, int value[], int value_lenght) {
    itab_expand_row(itab);
    for (int i = 0; i < value_lenght; i++) itab_append_col(itab, itab->row, value[i]);
    itab->row++;
}

int itab_get(itab_t *itab, int row, int col) {
  if ((row >= itab->row && col >= itab->col[row]) || row < 0 || col < 0) {
    printf("Index [%d][%d] out of bounds for itab_t of row size: %d, has col: %d\n", row, col, itab->row, itab->col[row]);
    exit(1);
  }
  return itab->data[row][col];
}

void itab_set(itab_t *itab, int row, int col, int value) {
  while (row > itab->row) {
    itab_add_row(itab);
  }

  while (col > itab->col[row]) {
    itab_append_col(itab, row, 0);
  }

  itab_append_col(itab, row, value);
}

void itab_expand_row(itab_t *itab) {
  if (itab->row >= itab->row_cap - 1) {
    int row_cap_before = itab->row_cap - 1;
    int new_row_cap = itab->row_cap * 2;
    int **newptr = (int **)realloc(itab->data, sizeof(int *) * new_row_cap);
    if (newptr == NULL) {
      itab_free(itab);
      printf("Failed Allocating New Memory\n");
      exit(1);
    }
    int *new_colptr = (int *)realloc(itab->col, sizeof(int) * new_row_cap);
    if (new_colptr == NULL) {
      itab_free(itab);
      printf("Failed Allocating New Memory For Column Index\n");
      exit(1);
    }
    itab->row_cap = new_row_cap;
    itab->data = newptr;
    itab->col = new_colptr;

    for (int r = row_cap_before; r < new_row_cap; r++) {
      itab->data[r] = (int *)malloc(sizeof(int) * itab->col_cap);
      if (itab->data[r] == 0) {
        itab_free(itab);
        printf("Failed Allocating Per Coloumn Memory, For Row: %d\n", r);
        exit(1);
      }
      itab->col[r] = 0;
    }
  }
}

void itab_expand_col(itab_t *itab, int row) {
  if (itab->col[row] >= itab->col_cap - 1) {
    itab->col_cap *= 2;
    for (int i = 0; i < itab->row_cap; i++) {
      itab->data[i] = (int *)realloc(itab->data[i], sizeof(int) * itab->col_cap);
    }
  }
}

int itab_sum_row(itab_t *itab, int col) {
  int sum = 0;
  for (int r = 0; r < itab->row; r++) {
    sum += itab_get(itab, r, col);
  }
  return sum;
}

int itab_sum_col(itab_t *itab, int row) {
  int sum = 0;
  for (int c = 0; c < itab->col[row]; c++) {
    sum += itab_get(itab, row, c);
  }
  return sum;
}

int itab_sum(itab_t *itab) {
  int sum = 0;
  for (int r = 0; r < itab->row; r++) {
    for (int c = 0; c < itab->col[r]; c++) {
      sum += itab_get(itab, r, c);
    }
  }
  return sum;
}

double itab_avg_row(itab_t *itab, int col) {
  double avg = 0.0;
  int counter = 0;
  for (int r = 0; r < itab->row; r++) {
    avg += (int)itab_get(itab, r, col);
    counter = counter + 1;
  }
  return avg / counter;
}

double itab_avg_col(itab_t *itab, int row) {
  double avg = 0.0;
  int counter = 0;
  for (int c = 0; c < itab->col[row]; c++) {
    avg += (int)itab_get(itab, row, c);
    counter = counter + 1;
  }
  return avg / counter;
}

double itab_avg(itab_t *itab) {
  double avg = 0.0;
  int counter = 0;
  for (int r = 0; r < itab->row; r++) {
    for (int c = 0; c < itab->col[r]; c++) {
      avg += (int)itab_get(itab, r, c);
      counter = counter + 1;
    }
  }
  return avg / counter;
}

void itab_prepend_col(itab_t *itab, int row, int value) {
  itab_append_col(itab, row, 0);
  for (int i = itab->col[row] - 1; i >= 0; i--) {
      itab->data[row][i] = itab->data[row][i - 1];
  }
  itab_set(itab, row, 0, value);
}

void itab_prepend_row(itab_t *itab, int value[], int lenght) {
  itab_add_row(itab);
  for (int r = itab->row; r > 0; r--) {
    itab->col[r] = (int) itab->col[r - 1];
    for (int c = 0; c < itab->col[r]; c++) {
      itab->data[r][c] = (int) itab->data[r - 1][c];
    }
  }
  itab->col[0] = lenght;
  for (int i = 0; i < lenght; i++)
    itab_set(itab, 0, i, value[i]);
}

void itab_insert_col(itab_t *itab, int index) {
  for (int r = 0; r < itab->row; r++) {
    itab_append_col(itab, r, 0);
    for (int c = itab->col[r]; c > index; c--) {
      itab->data[r][c] = itab->data[r][c - 1];
    }
    itab_set(itab, r, index, 0);
  }
}

void itab_insert_row(itab_t *itab, int index) {
  int vmax = vecmax(itab->col, itab->row);
  itab_add_row(itab);
  for (int r = itab->row; r > index; r--) {
    itab->col[r] = itab->col[r - 1];
    for (int c = 0; c < itab->col[r]; c++) {
      itab->data[r][c] = itab->data[r - 1][c];
    }
  }
  itab->col[index] = vmax;
  for (int i = 0; i < itab->col[index]; i++)
    itab_set(itab, index, i, 0.0);
}

void itab_pop_col(itab_t *itab) {
  int vmax = vecmax(itab->col, itab->row);
  itab_delete_col(itab, vmax - 1);
}

void itab_pop_row(itab_t *itab) {
  itab_delete_row(itab, itab->row);
}

void itab_delete_col(itab_t *itab, int col_index) {
  int vmax = vecmax(itab->col, itab->row);
  if (col_index >= vmax) {
    printf("Error - Max col Is %d\n", vmax);
    exit(1);
  } else {
    for (int r = 0; r < itab->row; r++) {
      for (int c = col_index; c < itab->col[r]; c++) {
          itab->data[r][c] = (int) itab->data[r][c + 1];
      }
      itab->col[r]--;
    }
  }
}

void itab_delete_row(itab_t *itab, int row_index) {
  if (row_index > itab->row) {
    printf("Error - Row Out Of Bounds, Max Row Is %d\n", itab->row);
    exit(1);
  } else {
    for (int r = row_index; r < itab->row; r++) {
      for (int c = 0; c < itab->col[r]; c++) {
          itab->data[r][c] = (int) itab->data[r + 1][c];
      }
      itab->col[r] = (int) itab->col[r + 1];
    }
    itab->row--;
  }
}

void itab_delete_cell(itab_t *itab, int row, int cols) {
    for (int c = cols; c < itab->col[row]; c++) {
        itab->data[row][c] = itab->data[row][c + 1];
    }
    itab->col[row]--;
}

void itab_wipeval(itab_t *itab, int value) {
  for (int r = 0; r < itab->row; r++){
    for (int c = 0; c < itab->col[r]; c++) {
        if (itab->data[r][c] == value){
            itab_delete_cell(itab, r, c);
            c--;
          }
    }
  }
}

int itab_search_col(itab_t *itab, int col, int value) {
  for (int r = 0; r < itab->row; r++) {
    if (itab->data[r][col] == value) {
        return r;
    }
  }
  return -1;
}

int itab_search_row(itab_t *itab, int row, int value) {
  for (int c = 0; c < itab->col[row]; c++) {
    if (itab->data[row][c] == value) {
        return c;
    }
  }
  return -1;
}

int itab_count(itab_t *itab, int value) {
  int counter = 0;
  for (int r = 0; r < itab->row; r++) {
    for (int c = 0; c < itab->col[r]; c++) {
        if (itab->data[r][c] == value) {
            counter++;
        }
    }
  }
  return counter;
}

void itab_swap_row(itab_t *itab, int row_source, int row_dest) {
  int temp;
  int temp_col;
  int vmax = vecmax(itab->col, itab->row);

  for (int c = 0; c < vmax; c++) {
    temp = itab->data[row_source][c];
    itab->data[row_source][c] = itab->data[row_dest][c];
    itab->data[row_dest][c] = temp;
  }
  
  temp_col = itab->col[row_source];
  itab->col[row_source] = itab->col[row_dest];
  itab->col[row_dest] = temp_col;
}

void itab_sort_asc(itab_t *itab) {
  int i,j;
  int swapped = 0;

  for(i = 0; i < itab->row; i++) { 
    swapped = 0;
    for(j = 0; j < itab->row - i; j++) {
       if(itab->data[j][0] > itab->data[j+1][0]) {
        itab_swap_row(itab, j, j + 1);
          swapped = 1;
       }
    }
    if(swapped == 0) {
       break;
    }
  }
}

void itab_sort_desc(itab_t *itab) {
  int i,j;
  int swapped = 0;

  for(i = 0; i < itab->row; i++) { 
    swapped = 0;
    for(j = 0; j < itab->row - i; j++) {
       if(itab->data[j][0] < itab->data[j+1][0]) {
        itab_swap_row(itab, j, j + 1);
          swapped = 1;
       }
    }
    if(swapped == 0) {
       break;
    }
  }
}

void itab_slice_row(itab_t *itab, int *dest, int row, int startpoint, int endpoint) {
  memcpy(dest, itab->data[row] + startpoint, (endpoint - startpoint + 1) * sizeof(*itab->data[row]));
}

void itab_filter(itab_t *itab, int filter_array[], int filter_lenght) {
  for (int i = filter_lenght - 1; i >= 0; i--) {
    itab_delete_row(itab, filter_array[i]);
  }
}

void itab_print(itab_t *itab) {
  int i;
  i = (vecmax(itab->col, itab->row + 1) > 9) ? 9 : vecmax(itab->col, itab->row + 1) - 1;
  int vmax = vecmax(itab->col, itab->row + 1);
  int maxcol = (vmax > 10) ? 6 : vmax - 1;
  int maxline = (vmax > 10) ? 10 : vmax - 1;
  int maxcol_bool = (vmax > 10) ? vmax : -100;
  int maxrow = (itab->row > 20) ? 10 : itab->row;
  int maxrow_bool = (itab->row > 20) ? itab->row : -100;

  // Header and Line
  printf("+");
  for (int s = 0; s < 12; s++) printf("-");
  for (int a = 0; a <= maxline; a++) {
    printf("+");
    for (int i = 0; i < 9; ++i) printf("-");
  }
  printf("+\n|  Row \\ Col |");
  for (i = 0; i <= maxcol; i++) printf("%5d    |", i);
  if (vmax > 10) printf("   ...   |");
  for (i = vmax - 3; i < maxcol_bool; i++) printf("%5d    |", i);
  printf("\n+");
  for (int s = 0; s < 12; s++) printf("-");
  for (int a = 0; a <= maxline; a++) {
    printf("+");
    for (int i = 0; i < 9; ++i) printf("-");
  }
  printf("+\n");

  // Row Index & Content
  for (int r = 0; r <= maxrow; r++) {
    printf("|%7d%5s|", r, " ");
    for (int c = 0; c <= maxcol; c++) if (c >= itab->col[r]) printf(" (null)  |"); else printf("%9d|", itab->data[r][c]);
    if (vmax > 10) printf("   ...   |");
    for (int c = vmax - 3; c < maxcol_bool; c++) if (c >= itab->col[r]) printf(" (null)  |"); else printf("%9d|", itab->data[r][c]);
    printf("\n");
  }
  if (itab->row > 20) {
    for (int r = 0; r < 3; r++) {
      printf("|%7s%5s|", ".", " ");
      for (int c = 0; c < maxline + 1; c++) printf("    .    |");
      printf("\n");
    }
  }
  for (int r = itab->row - 3; r < maxrow_bool; r++) {
    printf("|%7d%5s|", r, " ");
    for (int c = 0; c <= maxcol; c++) if (c >= itab->col[r]) printf(" (null)  |"); else printf("%9d|", itab->data[r][c]);
    if (vmax > 10) printf("   ...   |");
    for (int c = vmax - 3; c < maxcol_bool; c++) if (c >= itab->col[r]) printf(" (null)  |"); else printf("%9d|", itab->data[r][c]);
    printf("\n");
  }

  // Footer Line
  printf("+");
  for (int s = 0; s < 12; s++) printf("-");
  for (int a = 0; a <= maxline; a++){
    printf("+");
    for (int i = 0; i < 9; ++i) printf("-");
  }
  printf("+\n");
}

void itab_compact(itab_t *itab) {
  int n_row = (itab->row == 0) ? 1 : itab->row;
  int n_col = (vecmax(itab->col, itab->row) == 0) ? 1 : vecmax(itab->col, itab->row);
  itab->col = realloc(itab->col, sizeof(int) * n_row);
  itab->data = realloc(itab->data, sizeof(int *) * n_row);
  for (int i = 0; i < n_row; i++)
    itab->data[i] = realloc(itab->data[i], sizeof(int) * n_col);
}

void itab_copy(itab_t *from, itab_t *to) {
  if (to->ready == 1) itab_free(to);
  itab_init(to);
  for (int r = 0; r <= from->row; r++) {
    itab_add_row(to);
    for (int c = 0; c < from->col[r]; c++)
      itab_append_col(to, r, from->data[r][c]);
  }
  itab_pop_row(to);
  // itab_compact(to);
}

void itab_read_csv(itab_t *itab, char *csvfile, char *delimiter) {
  int rowIndex = 0;
  char line[2048];
  char* token = NULL;
  FILE* fp = fopen(csvfile,"r");
  if (fp != NULL) {
    while (fgets(line, sizeof(line), fp) != NULL) {
      // NOTE: Need to addrow first before append col, just like set() logic
      itab_add_row(itab);
      for (token = strtok(line, delimiter); token != NULL; token = strtok(NULL, delimiter))
        itab_append_col(itab, rowIndex, atoi(token));
      rowIndex++;
    }
    itab_pop_row(itab);
    fclose(fp);
  } else {
    printf("[ERROR] File %s Not Found!\n", csvfile);
    exit(0);
  }
}

void itab_serialize (itab_t *itab, char *name) {
    FILE *fp;
    char dir[64];
    strcpy(dir, SERIALIZE_DIR);
    strcat(dir, name);
    strcat(dir, ".itab");
    fp = fopen(dir, "wb");
    fwrite(&itab->row, sizeof(int), 1, fp);
    for (int r = 0; r < itab->row; r++) {
        fwrite(&itab->col[r], sizeof(int), 1, fp);
        for (int c = 0; c < itab->col[r]; c++) {
            fwrite(&itab->data[r][c], sizeof(int), 1, fp);
        }
    }
    fclose(fp);
}

void itab_deserialize (itab_t *itab, char *name) {
    FILE *fp;
    int temp = 0;
    int col = 0;
    int row = 0;
    char dir[64];
    strcpy(dir, SERIALIZE_DIR);
    strcat(dir, name);
    strcat(dir, ".itab");
    if (itab->row_cap > 0) itab_free(itab);
    itab_init(itab);
    fp = fopen(dir, "rb");
    fread(&row, sizeof(int), 1, fp);
    for (int r = 0; r < row; r++) {
        itab_add_row(itab);
        fread(&col, sizeof(int), 1, fp);
        for (int c = 0; c < col; c++) {
            fread(&temp, sizeof(int), 1, fp);
            itab_append_col(itab, r, temp);
        }
    }
    itab_pop_row(itab);
    fclose(fp);
}

void itab_free(itab_t *itab) {
  if (itab->ready == 1) {
    itab->ready = 0;
    for (int i = 0; i < itab->row; i++) free(itab->data[i]);
    free(itab->data);
  }
}

void dtab_init(dtab_t *dtab) {
  if (dtab->ready == 1) dtab_free(dtab);
  dtab->ready = 1;
  dtab->row = 0;
  dtab->row_cap = TABLE_INITIAL_ROW;
  dtab->col_cap = TABLE_INITIAL_COL;

  dtab->col = calloc(dtab->row_cap, sizeof(int) * dtab->row_cap);
  dtab->data = (double **)malloc(sizeof(double *) * dtab->row_cap);
  for (int i = 0; i < dtab->row_cap; i++)
    dtab->data[i] = (double *)malloc(sizeof(double) * dtab->col_cap);
}

void dtab_add_row(dtab_t *dtab) {
  dtab_expand_row(dtab);
  dtab->row++;
}

void dtab_append_col(dtab_t *dtab, int row, double value) {
  dtab_expand_col(dtab, row);
  dtab->data[row][dtab->col[row]++] = value;
}

void dtab_append_row(dtab_t *dtab, double *value, int value_lenght) {
    dtab_expand_row(dtab);
    for (int i = 0; i < value_lenght; i++) dtab_append_col(dtab, dtab->row, value[i]);
    dtab->row++;
}

double dtab_get(dtab_t *dtab, int row, int col) {
  if ((row >= dtab->row && col >= dtab->col[row]) || row < 0 || col < 0) {
    printf("Index [%d][%d] out of bounds for dtab_t of row size: %d, has col: %d\n", row, col, dtab->row, dtab->col[row]);
    exit(1);
  }
  return dtab->data[row][col];
}

void dtab_set(dtab_t *dtab, int row, int col, double value) {
  while (row > dtab->row) {
    dtab_add_row(dtab);
  }

  while (col > dtab->col[row]) {
    dtab_append_col(dtab, row, 0);
  }

  dtab_append_col(dtab, row, value);
}

void dtab_expand_row(dtab_t *dtab) {
  if (dtab->row >= dtab->row_cap - 1) {
    int row_cap_before = dtab->row_cap - 1;
    int new_row_cap = dtab->row_cap * 2;
    double **newptr = (double **)realloc(dtab->data, sizeof(double *) * new_row_cap);
    if (newptr == NULL) {
      dtab_free(dtab);
      printf("Failed Allocating New Memory\n");
      exit(1);
    }
    int *new_colptr = (int *)realloc(dtab->col, sizeof(int) * new_row_cap);
    if (new_colptr == NULL) {
      dtab_free(dtab);
      printf("Failed Allocating New Memory For Column Index\n");
      exit(1);
    }
    dtab->row_cap = new_row_cap;
    dtab->data = newptr;
    dtab->col = new_colptr;

    for (int r = row_cap_before; r < new_row_cap; r++) {
      dtab->data[r] = (double *)malloc(sizeof(double) * dtab->col_cap);
      if (dtab->data[r] == 0) {
        dtab_free(dtab);
        printf("Failed Allocating Per Coloumn Memory, For Row: %d\n", r);
        exit(1);
      }
      dtab->col[r] = 0;
    }
  }
}

void dtab_expand_col(dtab_t *dtab, int row) {
  if (dtab->col[row] >= dtab->col_cap - 1) {
    dtab->col_cap *= 2;
    for (int i = 0; i < dtab->row_cap; i++) {
      dtab->data[i] = (double *)realloc(dtab->data[i], sizeof(double) * dtab->col_cap);
    }
  }
}

double dtab_sum_row(dtab_t *dtab, int col) {
  double sum = 0.0;
  for (int r = 0; r < dtab->row; r++) {
    sum += dtab_get(dtab, r, col);
  }
  return sum;
}

double dtab_sum_col(dtab_t *dtab, int row) {
  double sum = 0.0;
  for (int c = 0; c < dtab->col[row]; c++) {
    sum += dtab_get(dtab, row, c);
  }
  return sum;
}

double dtab_sum(dtab_t *dtab) {
  double sum = 0.0;
  for (int r = 0; r < dtab->row; r++) {
    for (int c = 0; c < dtab->col[r]; c++) {
      sum += dtab_get(dtab, r, c);
    }
  }
  return sum;
}

double dtab_avg_row(dtab_t *dtab, int col) {
  double avg = 0.0;
  double counter = 0.0;
  for (int r = 0; r < dtab->row; r++) {
    avg += (double)dtab_get(dtab, r, col);
    counter = counter + 1.0;
  }
  return avg / counter;
}

double dtab_avg_col(dtab_t *dtab, int row) {
  double avg = 0.0;
  double counter = 0.0;
  for (int c = 0; c < dtab->col[row]; c++) {
    avg += (double)dtab_get(dtab, row, c);
    counter = counter + 1.0;
  }
  return avg / counter;
}

double dtab_avg(dtab_t *dtab) {
  double avg = 0.0;
  double counter = 0.0;
  for (int r = 0; r < dtab->row; r++) {
    for (int c = 0; c < dtab->col[r]; c++) {
      avg += (double)dtab_get(dtab, r, c);
      counter = counter + 1.0;
    }
  }
  return avg / counter;
}

void dtab_prepend_col(dtab_t *dtab, int row, double value) {
  dtab_append_col(dtab, row, 0);
  for (int i = dtab->col[row] - 1; i >= 0; i--) {
      dtab->data[row][i] = dtab->data[row][i - 1];
  }
  dtab_set(dtab, row, 0, value);
}

void dtab_prepend_row(dtab_t *dtab, double value[], int lenght) {
  dtab_add_row(dtab);
  for (int r = dtab->row; r > 0; r--) {
    dtab->col[r] = (int) dtab->col[r - 1];
    for (int c = 0; c < dtab->col[r]; c++) {
      dtab->data[r][c] = (double) dtab->data[r - 1][c];
    }
  }
  dtab->col[0] = lenght;
  for (int i = 0; i < lenght; i++)
    dtab_set(dtab, 0, i, value[i]);
}

void dtab_insert_col(dtab_t *dtab, int index) {
  for (int r = 0; r < dtab->row; r++) {
    dtab_append_col(dtab, r, 0);
    for (int c = dtab->col[r]; c > index; c--) {
      dtab->data[r][c] = dtab->data[r][c - 1];
    }
    dtab_set(dtab, r, index, 0);
  }
}

void dtab_insert_row(dtab_t *dtab, int index) {
  int vmax = vecmax(dtab->col, dtab->row);
  dtab_add_row(dtab);
  for (int r = dtab->row; r > index; r--) {
    dtab->col[r] = dtab->col[r - 1];
    for (int c = 0; c < dtab->col[r]; c++) {
      dtab->data[r][c] = dtab->data[r - 1][c];
    }
  }
  dtab->col[index] = vmax;
  for (int i = 0; i < dtab->col[index]; i++)
    dtab_set(dtab, index, i, 0.0);
}

void dtab_pop_col(dtab_t *dtab) {
  int vmax = vecmax(dtab->col, dtab->row);
  dtab_delete_col(dtab, vmax - 1);
}

void dtab_pop_row(dtab_t *dtab) {
  dtab_delete_row(dtab, dtab->row);
}

void dtab_delete_col(dtab_t *dtab, int col_index) {
  int vmax = vecmax(dtab->col, dtab->row);
  if (col_index >= vmax) {
    printf("Error - Max col Is %d\n", vmax);
    exit(1);
  } else {
    for (int r = 0; r < dtab->row; r++) {
      for (int c = col_index; c < dtab->col[r]; c++) {
          dtab->data[r][c] = (double) dtab->data[r][c + 1];
      }
      dtab->col[r]--;
    }
  }
}

void dtab_delete_row(dtab_t *dtab, int row_index) {
  if (row_index > dtab->row) {
    printf("Error - Row Out Of Bounds, Max Row Is %d\n", dtab->row);
    exit(1);
  } else {
    for (int r = row_index; r < dtab->row; r++) {
      for (int c = 0; c < dtab->col[r]; c++) {
          dtab->data[r][c] = (double) dtab->data[r + 1][c];
      }
      dtab->col[r] = (int) dtab->col[r + 1];
    }
    dtab->row--;
  }
}

void dtab_delete_cell(dtab_t *dtab, int row, int cols) {
    for (int c = cols; c < dtab->col[row]; c++) {
        dtab->data[row][c] = dtab->data[row][c + 1];
    }
    dtab->col[row]--;
}

void dtab_wipeval(dtab_t *dtab, double value) {
  for (int r = 0; r < dtab->row; r++){
    for (int c = 0; c < dtab->col[r]; c++) {
        if (dtab->data[r][c] == value){
            dtab_delete_cell(dtab, r, c);
            c--;
          }
    }
  }
}

int dtab_search_col(dtab_t *dtab, int col, double value) {
  for (int r = 0; r < dtab->row; r++) {
    if (dtab->data[r][col] == value) {
        return r;
    }
  }
  return -1;
}

int dtab_search_row(dtab_t *dtab, int row, double value) {
  for (int c = 0; c < dtab->col[row]; c++) {
    if (dtab->data[row][c] == value) {
        return c;
    }
  }
  return -1;
}

int dtab_count(dtab_t *dtab, double value) {
  int counter = 0;
  for (int r = 0; r < dtab->row; r++) {
    for (int c = 0; c < dtab->col[r]; c++) {
        if (dtab->data[r][c] == value) {
            counter++;
        }
    }
  }
  return counter;
}

void dtab_swap_row(dtab_t *dtab, int row_source, int row_dest) {
  double temp;
  int temp_col;
  int vmax = vecmax(dtab->col, dtab->row);

  for (int c = 0; c < vmax; c++) {
    temp = dtab->data[row_source][c];
    dtab->data[row_source][c] = dtab->data[row_dest][c];
    dtab->data[row_dest][c] = temp;
  }
  
  temp_col = dtab->col[row_source];
  dtab->col[row_source] = dtab->col[row_dest];
  dtab->col[row_dest] = temp_col;
}

void dtab_sort_asc(dtab_t *dtab) {
  int i,j;
  int swapped = 0;

  for(i = 0; i < dtab->row; i++) { 
    swapped = 0;
    for(j = 0; j < dtab->row - i; j++) {
       if(dtab->data[j][0] > dtab->data[j+1][0]) {
        dtab_swap_row(dtab, j, j + 1);
          swapped = 1;
       }
    }
    if(swapped == 0) {
       break;
    }
  }
}

void dtab_sort_desc(dtab_t *dtab) {
  int i,j;
  int swapped = 0;

  for(i = 0; i < dtab->row; i++) { 
    swapped = 0;
    for(j = 0; j < dtab->row - i; j++) {
       if(dtab->data[j][0] < dtab->data[j+1][0]) {
        dtab_swap_row(dtab, j, j + 1);
          swapped = 1;
       }
    }
    if(swapped == 0) {
       break;
    }
  }
}

void dtab_slice_row(dtab_t *dtab, double *dest, int row, int startpoint, int endpoint) {
  memcpy(dest, dtab->data[row] + startpoint, (endpoint - startpoint + 1) * sizeof(*dtab->data[row]));
}

void dtab_filter(dtab_t *dtab, int filter_array[], int filter_lenght) {
  for (int i = filter_lenght - 1; i >= 0; i--) {
    dtab_delete_row(dtab, filter_array[i]);
  }
}

void dtab_print(dtab_t *dtab) {
  int i;
  i = (vecmax(dtab->col, dtab->row + 1) > 9) ? 9 : vecmax(dtab->col, dtab->row + 1) - 1;
  int vmax = vecmax(dtab->col, dtab->row + 1);
  int maxcol = (vmax > 10) ? 6 : vmax - 1;
  int maxline = (vmax > 10) ? 10 : vmax - 1;
  int maxcol_bool = (vmax > 10) ? vmax : -100;
  int maxrow = (dtab->row > 20) ? 10 : dtab->row;
  int maxrow_bool = (dtab->row > 20) ? dtab->row : -100;

  // Header and Line
  printf("+");
  for (int s = 0; s < 12; s++) printf("-");
  for (int a = 0; a <= maxline; a++) {
    printf("+");
    for (int i = 0; i < 9; ++i) printf("-");
  }
  printf("+\n|  Row \\ Col |");
  for (i = 0; i <= maxcol; i++) printf("%5d    |", i);
  if (vmax > 10) printf("   ...   |");
  for (i = vmax - 3; i < maxcol_bool; i++) printf("%5d    |", i);
  printf("\n+");
  for (int s = 0; s < 12; s++) printf("-");
  for (int a = 0; a <= maxline; a++) {
    printf("+");
    for (int i = 0; i < 9; ++i) printf("-");
  }
  printf("+\n");

  // Row Index & Content
  for (int r = 0; r <= maxrow; r++) {
    printf("|%7d%5s|", r, " ");
    for (int c = 0; c <= maxcol; c++) if (c >= dtab->col[r]) printf(" (null)  |"); else printf("%9.6f|", dtab->data[r][c]);
    if (vmax > 10) printf("   ...   |");
    for (int c = vmax - 3; c < maxcol_bool; c++) if (c >= dtab->col[r]) printf(" (null)  |"); else printf("%9.6f|", dtab->data[r][c]);
    printf("\n");
  }
  if (dtab->row > 20) {
    for (int r = 0; r < 3; r++) {
      printf("|%7s%5s|", ".", " ");
      for (int c = 0; c < maxline + 1; c++) printf("    .    |");
      printf("\n");
    }
  }
  for (int r = dtab->row - 3; r < maxrow_bool; r++) {
    printf("|%7d%5s|", r, " ");
    for (int c = 0; c <= maxcol; c++) if (c >= dtab->col[r]) printf(" (null)  |"); else printf("%9.6f|", dtab->data[r][c]);
    if (vmax > 10) printf("   ...   |");
    for (int c = vmax - 3; c < maxcol_bool; c++) if (c >= dtab->col[r]) printf(" (null)  |"); else printf("%9.6f|", dtab->data[r][c]);
    printf("\n");
  }

  // Footer Line
  printf("+");
  for (int s = 0; s < 12; s++) printf("-");
  for (int a = 0; a <= maxline; a++){
    printf("+");
    for (int i = 0; i < 9; ++i) printf("-");
  }
  printf("+\n");
}

void dtab_compact(dtab_t *dtab) {
  int n_row = (dtab->row == 0) ? 1 : dtab->row;
  int n_col = (vecmax(dtab->col, dtab->row) == 0) ? 1 : vecmax(dtab->col, dtab->row);
  dtab->col = realloc(dtab->col, sizeof(int) * n_row);
  dtab->data = realloc(dtab->data, sizeof(double *) * n_row);
  for (int i = 0; i < n_row; i++)
    dtab->data[i] = realloc(dtab->data[i], sizeof(double) * n_col);
}

void dtab_copy(dtab_t *from, dtab_t *to) {
  if (to->ready == 1) dtab_free(to);
  dtab_init(to);
  for (int r = 0; r <= from->row; r++) {
    dtab_add_row(to);
    for (int c = 0; c < from->col[r]; c++)
      dtab_append_col(to, r, from->data[r][c]);
  }
  dtab_pop_row(to);
  // dtab_compact(to);
}

void dtab_read_csv(dtab_t *dtab, char *csvfile, char *delimiter) {
  int rowIndex = 0;
  char line[2048];
  char* token = NULL;
  FILE* fp = fopen(csvfile,"r");
  if (fp != NULL) {
    while (fgets(line, sizeof(line), fp) != NULL) {
      // NOTE: Need to addrow first before append col, just like set() logic
      dtab_add_row(dtab);
      for (token = strtok(line, delimiter); token != NULL; token = strtok(NULL, delimiter))
        dtab_append_col(dtab, rowIndex, atof(token));
      rowIndex++;
    }
    dtab_pop_row(dtab);
    fclose(fp);
  } else {
    printf("[ERROR] File %s Not Found!\n", csvfile);
    exit(0);
  }
}

void dtab_serialize (dtab_t *dtab, char *name) {
    FILE *fp;
    char dir[64];
    strcpy(dir, SERIALIZE_DIR);
    strcat(dir, name);
    strcat(dir, ".dtab");
    fp = fopen(dir, "wb");
    fwrite(&dtab->row, sizeof(int), 1, fp);
    for (int r = 0; r < dtab->row; r++) {
        fwrite(&dtab->col[r], sizeof(int), 1, fp);
        for (int c = 0; c < dtab->col[r]; c++) {
            fwrite(&dtab->data[r][c], sizeof(double), 1, fp);
        }
    }
    fclose(fp);
}

void dtab_deserialize (dtab_t *dtab, char *name) {
    FILE *fp;
    double temp = 0.0;
    int col = 0;
    int row = 0;
    char dir[64];
    strcpy(dir, SERIALIZE_DIR);
    strcat(dir, name);
    strcat(dir, ".dtab");
    if (dtab->row_cap > 0) dtab_free(dtab);
    dtab_init(dtab);
    fp = fopen(dir, "rb");
    fread(&row, sizeof(int), 1, fp);
    for (int r = 0; r < row; r++) {
        dtab_add_row(dtab);
        fread(&col, sizeof(int), 1, fp);
        for (int c = 0; c < col; c++) {
            fread(&temp, sizeof(double), 1, fp);
            dtab_append_col(dtab, r, temp);
        }
    }
    dtab_pop_row(dtab);
    fclose(fp);
}

void dtab_free(dtab_t *dtab) {
  if (dtab->ready == 1) {
    dtab->ready = 0;
    for (int i = 0; i < dtab->row; i++) free(dtab->data[i]);
    free(dtab->data);
  }
}