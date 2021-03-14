#include <stdio.h>
#include <stdlib.h>

#define VEC_INITIAL_CAPACITY 50
#define STRING_INITIAL_SIZE 20
#define STRING_INITIAL_LEN 16
#define SERIALIZE_DIR "../res/"
#define PRINT_MAX 20
#define PRINT_ROW 5
#define vec_name(name)  #name

// Define a ivec_t type
typedef struct {
  int ready;
  int size;
  int capacity;
  int *data;
} ivec_t; // Integer-Vector

typedef struct {
  int ready;
  int size;
  int capacity;
  double *data;
} dvec_t; // Double-Vector

typedef struct {
  int ready;
  int line;
  int linecap;
  int *len;
  int lencap;
  char **data;
} strvec_t; // String-Vector

int vectormax(int *array, int arraySize);

// ivec_t Header
void ivec_init(ivec_t *ivec);
void ivec_append(ivec_t *ivec, int value);
int ivec_get(ivec_t *ivec, int index);
void ivec_set(ivec_t *ivec, int index, int value);
void ivec_double_capacity_if_full(ivec_t *ivec);
int ivec_sum(ivec_t *ivec);
double ivec_avg(ivec_t *ivec);
void ivec_prepend(ivec_t *ivec, int value);
int ivec_pop(ivec_t *ivec);
void ivec_delete(ivec_t *ivec, int index);
void ivec_wipeval(ivec_t *ivec, int value);
int ivec_search(ivec_t *ivec, int value);
void ivec_sort_asc(ivec_t *ivec);
void ivec_sort_desc(ivec_t *ivec);
void ivec_slice(ivec_t *ivec, int *dest, int startpoint, int endpoint);
void ivec_filter(ivec_t *ivec, int filter_array[], int filter_lenght);
void ivec_copy(ivec_t *from, ivec_t *to);
void ivec_from_string (ivec_t *ivec, char *str_data);
void ivec_print(ivec_t *ivec);
void ivec_serialize (ivec_t *ivec, char *name);
void ivec_deserialize (ivec_t *ivec, char *name);
int ivec_count(ivec_t *ivec, int value);
int ivec_max(ivec_t *ivec);
int ivec_min(ivec_t *ivec);
int ivec_argmax(ivec_t *ivec);
int ivec_argmin(ivec_t *ivec);
void ivec_scaler(ivec_t *ivec);
int ivec_matmul(ivec_t *ivec1, ivec_t *ivec2);
void ivec_matadd(ivec_t *ivec1, ivec_t *ivec2);
void ivec_free(ivec_t *ivec);

// dvec_t Header
void dvec_init(dvec_t *dvec);
void dvec_append(dvec_t *dvec, double value);
double dvec_get(dvec_t *dvec, int index);
void dvec_set(dvec_t *dvec, int index, double value);
void dvec_double_capacity_if_full(dvec_t *dvec);
double dvec_sum(dvec_t *dvec);
double dvec_avg(dvec_t *dvec);
void dvec_prepend(dvec_t *dvec, double value);
double dvec_pop(dvec_t *dvec);
void dvec_delete(dvec_t *dvec, int index);
void dvec_wipeval(dvec_t *dvec, double value);
int dvec_search(dvec_t *dvec, double value);
void dvec_sort_asc(dvec_t *dvec);
void dvec_sort_desc(dvec_t *dvec);
void dvec_slice(dvec_t *dvec, double *dest, int startpoint, int endpoint);
void dvec_filter(dvec_t *ivec, int filter_array[], int filter_lenght);
void dvec_copy(dvec_t *from, dvec_t *to);
void dvec_from_string (dvec_t *dvec, char *str_data);
void dvec_print(dvec_t *dvec);
void dvec_serialize (dvec_t *dvec, char *name);
void dvec_deserialize (dvec_t *dvec, char *name);
int dvec_count(dvec_t *dvec, double value);
double dvec_max(dvec_t *dvec);
double dvec_min(dvec_t *dvec);
int dvec_argmax(dvec_t *dvec);
int dvec_argmin(dvec_t *dvec);
void dvec_scaler(dvec_t *dvec);
double dvec_matmul(dvec_t *dvec1, dvec_t *dvec2);
void dvec_matadd(dvec_t *dvec1, dvec_t *dvec2);
void dvec_free(dvec_t *dvec);

// strvec_t Header
void strvec_init(strvec_t *strvec);
void strvec_add_line(strvec_t *strvec);
void strvec_concat(strvec_t *strvec, int line, char *value);
void strvec_append_line(strvec_t *strvec, char *value);
const char *strvec_get(strvec_t *strvec, int line);
void strvec_set(strvec_t *strvec, int line, char *value);
void strvec_expand_line(strvec_t *strvec);
void strvec_expand_len(strvec_t *strvec, int line, int estimated_len);
void strvec_prepend (strvec_t *strvec, int line, char *value);
void strvec_prepend_line(strvec_t *strvec, char *value);
void strvec_insert(strvec_t *strvec, int line);
void strvec_pop(strvec_t *strvec);
void strvec_delete(strvec_t *strvec, int line_index);
void strvec_wipeval(strvec_t *strvec, char *value);
int strvec_search(strvec_t *strvec, char *value);
int strvec_count(strvec_t *strvec, char *value);
void strvec_swap_line(strvec_t *strvec, int line_source, int line_dest);
void strvec_sort_asc(strvec_t *strvec);
void strvec_sort_desc(strvec_t *strvec);
void strvec_print(strvec_t *strvec);
void strvec_compact(strvec_t *strvec);
void strvec_free(strvec_t *strvec);

int vectormax(int *array, int arraySize) {
    int ret = array[0];
    for (int i = 0; i < arraySize; i++) {if (array[i] > ret) ret = array[i];}
    return ret;
}

void ivec_init(ivec_t *ivec) {
  if (ivec->ready == 1) ivec_free(ivec);
  ivec->ready = 1;
  // initialize size and capacity
  ivec->size = 0;
  ivec->capacity = VEC_INITIAL_CAPACITY;

  // allocate memory for ivec->data
  ivec->data = malloc(sizeof(int) * ivec->capacity);
}

void ivec_append(ivec_t *ivec, int value) {
  // make sure there's room to expand into
  ivec_double_capacity_if_full(ivec);

  // append the value and increment ivec->size
  ivec->data[ivec->size++] = value;
}

int ivec_get(ivec_t *ivec, int index) {
  if (index >= ivec->size || index < 0) {
    printf("Index %d out of bounds for ivec_t of size %d\n", index, ivec->size);
    exit(1);
  }
  return ivec->data[index];
}

void ivec_set(ivec_t *ivec, int index, int value) {
  // zero fill the ivec_t up to the desired index
  while (index >= ivec->size) {
    ivec_append(ivec, 0);
  }

  // set the value at the desired index
  ivec->data[index] = value;
}

void ivec_double_capacity_if_full(ivec_t *ivec) {
  if (ivec->size >= ivec->capacity) {
    // double ivec->capacity and resize the allocated memory accordingly
    ivec->capacity *= 2;
    ivec->data = realloc(ivec->data, sizeof(int) * ivec->capacity);
  }
}

int ivec_sum(ivec_t *ivec) {
  int sum = 0;
  for (int i = 0; i < ivec->size; i++) {
    sum += ivec_get(ivec, i);
  }
  return sum;
}

double ivec_avg(ivec_t *ivec) {
  double avg = 0.0;
  for (int i = 0; i < ivec->size; i++) {
    avg += (double)ivec_get(ivec, i);
  }
  return avg / ivec->size;
}

void ivec_prepend(ivec_t *ivec, int value) {
    ivec_append(ivec, 0);
    for (int i = ivec->size - 1; i >= 0; i--) {
        ivec->data[i] = ivec->data[i - 1];
    }
    ivec_set(ivec, 0, value);
}

int ivec_pop(ivec_t *ivec) {
    int data = ivec->data[ivec->size - 1];
    ivec_set(ivec, ivec->size, 0);
    ivec->size = ivec->size - 2;
    return data;
}

void ivec_delete(ivec_t *ivec, int index) {
    for (int i = 0; i < ivec->size - index; i++) {
        ivec->data[index + i] = ivec->data[index + i + 1];
    }
    ivec->size = ivec->size - 1;
}

void ivec_wipeval(ivec_t *ivec, int value) {
    for (int i = 0; i < ivec->size; i++) {
        if (ivec->data[i] == value) {
            ivec_delete(ivec, i);
        }
    }
}

int ivec_search(ivec_t *ivec, int value) {
    for (int i = 0; i < ivec->size; i++) {
        if (ivec->data[i] == value) {
            return i;
        }
    }
    return -1;
}

void ivec_sort_asc(ivec_t *ivec) {
   int temp;
   int i,j;
   bool swapped = false;
   
   for(i = 0; i < ivec->size; i++) { 
      swapped = false;
      for(j = 0; j < ivec->size - i; j++) {
         if(ivec->data[j] > ivec->data[j+1]) {
            temp = ivec->data[j];
            ivec->data[j] = ivec->data[j+1];
            ivec->data[j+1] = temp;
            swapped = true;
         }
      }
      if(!swapped) {
         break;
      }
   }
}

void ivec_sort_desc(ivec_t *ivec) {
   int temp;
   int i,j;
   bool swapped = false;
   
   for(i = 0; i < ivec->size; i++) { 
      swapped = false;
      for(j = 0; j < ivec->size - i; j++) {
         if(ivec->data[j] < ivec->data[j+1]) {
            temp = ivec->data[j];
            ivec->data[j] = ivec->data[j+1];
            ivec->data[j+1] = temp;
            swapped = true;
         }
      }
      if(!swapped) {
         break;
      }
   }
}

void ivec_slice(ivec_t *ivec, int *dest, int startpoint, int endpoint) {
  memcpy(dest, ivec->data + startpoint, (endpoint - startpoint + 1) * sizeof(*ivec->data));
}

void ivec_filter(ivec_t *ivec, int filter_array[], int filter_lenght) {
  for (int i = filter_lenght - 1; i >= 0; i--) {
    ivec_delete(ivec, filter_array[i]);
  }
}

void ivec_copy(ivec_t *from, ivec_t *to) {
  ivec_init(to);
  for(int i = 0; i < from->size; i++) {
    ivec_append(to, from->data[i]);
  }
}

void ivec_from_string (ivec_t *ivec, char *str_data) {
    if (ivec->capacity > 0) {
        ivec_free(ivec);
    }
    ivec_init(ivec);
    char *token;
    char temp[1024];
    strcpy(temp, str_data);
    for (token = strtok(temp, ","); token != NULL; token = strtok(NULL, ",")) {
        ivec_append(ivec, atof(token));
    }
}

void ivec_print(ivec_t *ivec) {
  int sizemax = ivec->size > PRINT_MAX ? PRINT_MAX : ivec->size;
  int sizebool = ivec->size > PRINT_MAX ? PRINT_ROW * 2 : 0;
  int sizefoot = ivec->size > PRINT_MAX ? ivec->size : 0;
  int startfoot = ivec->size - (PRINT_ROW * 2) - ivec->size % PRINT_ROW;
  startfoot = startfoot < 0 ? 0 : startfoot;
  printf("%5s(%7d ) {[\t", "ivec", ivec->size);
  for (int i = 0; i < sizemax; i++) {
    if (i % PRINT_ROW == 0 && i != 0) {
      printf("\n\t\t\t\t\t");
    }
    printf("%7d,\t", ivec->data[i]);
  }
  if (sizebool > 0) printf("\n\t\t\t\t\t");
  for (int i = 0; i < sizebool; i++) {
    if (i % PRINT_ROW == 0 && i != 0) {
      printf("\n\t\t\t\t\t");
    }
    printf("%7s \t", ".   ");
  }
  if (sizefoot > 0) printf("\n\t\t\t\t\t");
  for (int i = startfoot; i < sizefoot; i++) {
    if (i % PRINT_ROW == 0 && i != 0) {
      printf("\n\t\t\t\t\t");
    }
    printf("%7d,\t", ivec->data[i]);
  }
  printf("]}\n");
}

void ivec_serialize (ivec_t *ivec, char *name) {
    FILE *fp;
    char dir[64];
    strcpy(dir, SERIALIZE_DIR);
    strcat(dir, name);
    strcat(dir, ".ivec");
    fp = fopen(dir, "wb");
    fwrite(&ivec->size, sizeof(int), 1, fp);
    for (int i = 0; i < ivec->size; i++) {
        fwrite(&ivec->data[i], sizeof(int), 1, fp);
    }
    fclose(fp);
}

void ivec_deserialize (ivec_t *ivec, char *name) {
    FILE *fp;
    int temp = 0;
    int size = 0;
    char dir[64];
    strcpy(dir, SERIALIZE_DIR);
    strcat(dir, name);
    strcat(dir, ".ivec");
    if (ivec->capacity > 0) ivec_free(ivec);
    ivec_init(ivec);
    fp = fopen(dir, "rb");
    fread(&size, sizeof(int), 1, fp);
    for (int i = 0; i < size; i++) {
        fread(&temp, sizeof(int), 1, fp);
        ivec_append(ivec, temp);
    }
    fclose(fp);
}

int ivec_count(ivec_t *ivec, int value) {
  int counter = 0;
  for (int i = 0; i <= ivec->size; i++) {
    if (ivec->data[i] == value) {
        counter++;
    }
  }
  return counter;
}

int ivec_max(ivec_t *ivec) {
  int ret = ivec->data[0];
  for (int i = 0; i < ivec->size; i++) {
        if (ivec->data[i] > ret) {
            ret = ivec->data[i];
        }
    }
    return ret;
}

int ivec_min(ivec_t *ivec) {
  int ret = ivec->data[0];
  for (int i = 0; i < ivec->size; i++) {
        if (ivec->data[i] < ret) {
            ret = ivec->data[i];
        }
    }
    return ret;
}

int ivec_argmax(ivec_t *ivec) {
  return ivec_search(ivec, ivec_max(ivec));
}

int ivec_argmin(ivec_t *ivec) {
  return ivec_search(ivec, ivec_min(ivec));
}

void ivec_scaler(ivec_t *ivec) {
  int min = ivec_min(ivec);
  int max = ivec_max(ivec);
  for (int i = 0; i < ivec->size; i++)
    ivec->data[i] = ((ivec->data[i] - min) / (max - min)) * 100;
}

int ivec_matmul(ivec_t *ivec1, ivec_t *ivec2) {
  if (ivec1->size != ivec2->size) {
    printf("[ERROR] - Vector Size Does Not Same, Can't Proceed Matrix Multiplication!\n");
    exit(0);
  }
  int ret = 0;
  for (int i = 0; i < ivec1->size; i++) {
    ret += ivec1->data[i] * ivec2->data[i];
  }
  return ret;
}

void ivec_matadd(ivec_t *ivec1, ivec_t *ivec2) {
  if (ivec1->size != ivec2->size) {
    printf("[ERROR] - Vector Size Does Not Same, Can't Proceed Matrix Addition!\n");
    exit(0);
  }
  for (int i = 0; i < ivec1->size; i++) {
    ivec1->data[i] = ivec1->data[i] + ivec2->data[i];
  }
}

void ivec_free(ivec_t *ivec) {
  if (ivec->ready == 1) {
    ivec->ready = 0;
    free(ivec->data);
  }
}

void dvec_init(dvec_t *dvec) {
  if (dvec->ready == 1) dvec_free(dvec);
  dvec->ready = 1;
  // initialize size and capacity
  dvec->size = 0;
  dvec->capacity = VEC_INITIAL_CAPACITY;

  // allocate memory for dvec->data
  dvec->data = malloc(sizeof(double) * dvec->capacity);
}

void dvec_append(dvec_t *dvec, double value) {
  // make sure there's room to expand into
  dvec_double_capacity_if_full(dvec);

  // append the value and increment dvec->size
  dvec->data[dvec->size++] = value;
}

double dvec_get(dvec_t *dvec, int index) {
  if (index >= dvec->size || index < 0) {
    printf("Index %d out of bounds for dvec_t of size %d\n", index, dvec->size);
    exit(1);
  }
  return dvec->data[index];
}

void dvec_set(dvec_t *dvec, int index, double value) {
  // zero fill the dvec_t up to the desired index
  while (index >= dvec->size) {
    dvec_append(dvec, 0);
  }

  // set the value at the desired index
  dvec->data[index] = value;
}

void dvec_double_capacity_if_full(dvec_t *dvec) {
  if (dvec->size >= dvec->capacity) {
    // double dvec->capacity and resize the allocated memory accordingly
    dvec->capacity *= 2;
    dvec->data = realloc(dvec->data, sizeof(double) * dvec->capacity);
  }
}

double dvec_sum(dvec_t *dvec) {
  double sum = 0;
  for (int i = 0; i < dvec->size; i++) {
    sum += dvec_get(dvec, i);
  }
  return sum;
}

double dvec_avg(dvec_t *dvec) {
  double avg = 0.0;
  for (int i = 0; i < dvec->size; i++) {
    avg += dvec_get(dvec, i);
  }
  return avg / dvec->size;
}

void dvec_prepend(dvec_t *dvec, double value) {
    dvec_append(dvec, 0);
    for (int i = dvec->size - 1; i >= 0; i--) {
        dvec->data[i] = dvec->data[i - 1];
    }
    dvec_set(dvec, 0, value);
}

double dvec_pop(dvec_t *dvec) {
    double data = dvec->data[dvec->size - 1];
    dvec_set(dvec, dvec->size, 0);
    dvec->size = dvec->size - 2;
    return data;
}

void dvec_delete(dvec_t *dvec, int index) {
    for (int i = 0; i < dvec->size - index; i++) {
        dvec->data[index + i] = dvec->data[index + i + 1];
    }
    dvec->size = dvec->size - 1;
}

void dvec_wipeval(dvec_t *dvec, double value) {
    for (int i = 0; i < dvec->size; i++) {
        if (dvec->data[i] == value) {
            dvec_delete(dvec, i);
        }
    }
}

int dvec_search(dvec_t *dvec, double value) {
    for (int i = 0; i < dvec->size; i++) {
        if (dvec->data[i] == value) {
            return i;
        }
    }
    return -1;
}

void dvec_sort_asc(dvec_t *dvec) {
   double temp;
   int i,j;
   bool swapped = false;
   
   for(i = 0; i < dvec->size; i++) { 
      swapped = false;
      for(j = 0; j < dvec->size - i; j++) {
         if(dvec->data[j] > dvec->data[j+1]) {
            temp = dvec->data[j];
            dvec->data[j] = dvec->data[j+1];
            dvec->data[j+1] = temp;
            swapped = true;
         }
      }
      if(!swapped) {
         break;
      }
   }
}

void dvec_sort_desc(dvec_t *dvec) {
   double temp;
   int i,j;
   bool swapped = false;
   
   for(i = 0; i < dvec->size; i++) { 
      swapped = false;
      for(j = 0; j < dvec->size - i; j++) {
         if(dvec->data[j] < dvec->data[j+1]) {
            temp = dvec->data[j];
            dvec->data[j] = dvec->data[j+1];
            dvec->data[j+1] = temp;
            swapped = true;
         }
      }
      if(!swapped) {
         break;
      }
   }
}

void dvec_slice(dvec_t *dvec, double *dest, int startpoint, int endpoint) {
  memcpy(dest, dvec->data + startpoint, (endpoint - startpoint + 1) * sizeof(*dvec->data));
}

void dvec_filter(dvec_t *ivec, int filter_array[], int filter_lenght) {
  for (int i = filter_lenght - 1; i >= 0; i--) {
    dvec_delete(ivec, filter_array[i]);
  }
}

void dvec_copy(dvec_t *from, dvec_t *to) {
  dvec_init(to);
  for(int i = 0; i < from->size; i++) {
    dvec_append(to, from->data[i]);
  }
}

void dvec_from_string (dvec_t *dvec, char *str_data) {
    if (dvec->capacity > 0) {
        dvec_free(dvec);
    }
    dvec_init(dvec);
    char *token;
    char temp[1024];
    strcpy(temp, str_data);
    for (token = strtok(temp, ","); token != NULL; token = strtok(NULL, ",")) {
        dvec_append(dvec, atof(token));
    }
}

void dvec_print(dvec_t *dvec) {
  int sizemax = dvec->size > PRINT_MAX ? PRINT_MAX : dvec->size;
  int sizebool = dvec->size > PRINT_MAX ? PRINT_ROW * 2 : 0;
  int sizefoot = dvec->size > PRINT_MAX ? dvec->size : 0;
  int startfoot = dvec->size - (PRINT_ROW * 2) - dvec->size % PRINT_ROW;
  startfoot = startfoot < 0 ? 0 : startfoot;
  printf("%5s(%7d ) {[\t", "dvec", dvec->size);
  for (int i = 0; i < sizemax; i++) {
    if (i % PRINT_ROW == 0 && i != 0) {
      printf("\n\t\t\t\t\t");
    }
    printf("%7.4f,\t", dvec->data[i]);
  }
  if (sizebool > 0) printf("\n\t\t\t\t\t");
  for (int i = 0; i < sizebool; i++) {
    if (i % PRINT_ROW == 0 && i != 0) {
      printf("\n\t\t\t\t\t");
    }
    printf("%7s \t", ".   ");
  }
  if (sizefoot > 0) printf("\n\t\t\t\t\t");
  for (int i = startfoot; i < sizefoot; i++) {
    if (i % PRINT_ROW == 0 && i != 0) {
      printf("\n\t\t\t\t\t");
    }
    printf("%7.4f,\t", dvec->data[i]);
  }
  printf("]}\n");
}

void dvec_serialize (dvec_t *dvec, char *name) {
    FILE *fp;
    char dir[64];
    strcpy(dir, SERIALIZE_DIR);
    strcat(dir, name);
    strcat(dir, ".dvec");
    fp = fopen(dir, "wb");
    fwrite(&dvec->size, sizeof(int), 1, fp);
    for (int i = 0; i < dvec->size; i++) {
        fwrite(&dvec->data[i], sizeof(double), 1, fp);
    }
    fclose(fp);
}

void dvec_deserialize (dvec_t *dvec, char *name) {
    FILE *fp;
    double temp = 0.0;
    int size = 0;
    char dir[64];
    strcpy(dir, SERIALIZE_DIR);
    strcat(dir, name);
    strcat(dir, ".dvec");
    if (dvec->capacity > 0) dvec_free(dvec);
    dvec_init(dvec);
    fp = fopen(dir, "rb");
    fread(&size, sizeof(int), 1, fp);
    for (int i = 0; i < size; i++) {
        fread(&temp, sizeof(double), 1, fp);
        dvec_append(dvec, temp);
    }
    fclose(fp);
}

int dvec_count(dvec_t *dvec, double value) {
  int counter = 0;
  for (int i = 0; i <= dvec->size; i++) {
    if (dvec->data[i] == value) {
        counter++;
    }
  }
  return counter;
}

double dvec_max(dvec_t *dvec) {
  double ret = dvec->data[0];
  for (int i = 0; i < dvec->size; i++) {
        if (dvec->data[i] > ret) {
            ret = dvec->data[i];
        }
    }
    return ret;
}

double dvec_min(dvec_t *dvec) {
  double ret = dvec->data[0];
  for (int i = 0; i < dvec->size; i++) {
        if (dvec->data[i] < ret) {
            ret = dvec->data[i];
        }
    }
    return ret;
}

int dvec_argmax(dvec_t *dvec) {
  return dvec_search(dvec, dvec_max(dvec));
}

int dvec_argmin(dvec_t *dvec) {
  return dvec_search(dvec, dvec_min(dvec));
}

void dvec_scaler(dvec_t *dvec) {
  double min = dvec_min(dvec);
  double max = dvec_max(dvec);
  for (int i = 0; i < dvec->size; i++)
    dvec->data[i] = ((dvec->data[i] - min) / (max - min));
}

double dvec_matmul(dvec_t *dvec1, dvec_t *dvec2) {
  if (dvec1->size != dvec2->size) {
    printf("[ERROR] - Vector Size Does Not Same, Can't Proceed Matrix Multiplication!\n");
    exit(0);
  }
  double ret = 0;
  for (int i = 0; i < dvec1->size; i++) {
    ret += dvec1->data[i] * dvec2->data[i];
  }
  return ret;
}

void dvec_matadd(dvec_t *dvec1, dvec_t *dvec2) {
  if (dvec1->size != dvec2->size) {
    printf("[ERROR] - Vector Size Does Not Same, Can't Proceed Matrix Addition!\n");
    exit(0);
  }
  for (int i = 0; i < dvec1->size; i++) {
    dvec1->data[i] = dvec1->data[i] + dvec2->data[i];
  }
}

void dvec_free(dvec_t *dvec) {
  if (dvec->ready == 1) {
    dvec->ready = 0;
    free(dvec->data);
  }
}

void strvec_init(strvec_t *strvec) {
  if (strvec->ready == 1) strvec_free(strvec);
  strvec->ready = 1;
  strvec->line = 0;
  strvec->linecap = STRING_INITIAL_SIZE;
  strvec->lencap = STRING_INITIAL_LEN;

  strvec->len = calloc(strvec->linecap, sizeof(int) * strvec->linecap);
  strvec->data = (char **)malloc(sizeof(char *) * strvec->linecap);
  for (int i = 0; i < strvec->linecap; i++)
    strvec->data[i] = (char *)malloc(sizeof(char) * strvec->lencap);

  strvec->data[0][0] = '\0';
}

void strvec_add_line(strvec_t *strvec) {
  strvec->line++;
  strvec_expand_line(strvec);
  strvec->data[strvec->line][0] = '\0';
}

void strvec_concat(strvec_t *strvec, int line, char *value) {
  strvec_expand_len(strvec, line, strlen(value));
  strcat(strvec->data[line], value);
  strvec->len[line] = strlen(strvec->data[line]);
}

void strvec_append_line(strvec_t *strvec, char *value) {
    strvec_expand_line(strvec);
    strvec_expand_len(strvec, strvec->line, strlen(value));
    strcpy(strvec->data[strvec->line], value);
    strvec->line++;
}

const char *strvec_get(strvec_t *strvec, int line) {
  if ((line >= strvec->line) || line < 0) {
    printf("Index [%d] out of bounds for strvec_t of line size: %d\n", line, strvec->line);
    exit(1);
  }
  return strvec->data[line];
}

void strvec_set(strvec_t *strvec, int line, char *value) {
  while (line > strvec->line) {
    strvec_add_line(strvec);
  }

  strvec_expand_len(strvec, line, strlen(value));

  strcpy(strvec->data[line], value);
}

void strvec_expand_line(strvec_t *strvec) {
  if (strvec->line + 1 >= strvec->linecap) {
    int line_cap_before = strvec->linecap - 1;
    int new_line_cap = strvec->linecap * 2;
    strvec->data = (char **)realloc(strvec->data, sizeof(char *) * new_line_cap);
    if (strvec->data == NULL) {
      strvec_free(strvec);
      printf("Failed Allocating New Memory\n");
      exit(1);
    }
    strvec->len = (int *)realloc(strvec->len, sizeof(int) * new_line_cap);
    if (strvec->len == NULL) {
      strvec_free(strvec);
      printf("Failed Allocating New Memory For Additional String Lenght Information\n");
      exit(1);
    }
    strvec->linecap = new_line_cap;

    for (int r = line_cap_before; r < new_line_cap; r++) {
      strvec->data[r] = (char *)malloc(sizeof(char) * strvec->lencap);
      if (strvec->data[r] == 0) {
        strvec_free(strvec);
        printf("Failed Allocating Per Coloumn Memory, For line: %d\n", r);
        exit(1);
      }
      strvec->len[r] = 0;
    }
  }
}

void strvec_expand_len(strvec_t *strvec, int line, int estimated_len) {
  if (strvec->len[line] + estimated_len >= strvec->lencap) {
    strvec->lencap = estimated_len;
    for (int i = 0; i < strvec->linecap; i++) {
      strvec->data[i] = (char *)realloc(strvec->data[i], sizeof(char) * strvec->lencap);
    }
  }
}

void strvec_prepend (strvec_t *strvec, int line, char *value) {
    size_t len = strlen(value);
    size_t i;
    memmove(strvec->data[line] + len, strvec->data[line], strlen(strvec->data[line]) + 1);
    for (i = 0; i < len; ++i) strvec->data[line][i] = value[i];
}

void strvec_prepend_line(strvec_t *strvec, char *value) {
  strvec_add_line(strvec);
  for (int r = strvec->line; r > 0; r--) {
    strvec->len[r] = (char) strvec->len[r - 1];
    for (int c = 0; c < strvec->len[r]; c++) {
      strvec->data[r][c] = (char) strvec->data[r - 1][c];
    }
  }
  strvec->len[0] = strlen(value);
  strcpy(strvec->data[0], value);
}

void strvec_insert(strvec_t *strvec, int line) {
  int vmax = vectormax(strvec->len, strvec->line);
  strvec_add_line(strvec);
  for (int r = strvec->line; r > line; r--) {
    strvec->len[r] = strvec->len[r - 1];
    for (int c = 0; c < strvec->len[r]; c++) {
      strvec->data[r][c] = strvec->data[r - 1][c];
    }
  }
  strvec->len[line] = vmax;
  strvec->data[line][0] = '\0';
}

void strvec_pop(strvec_t *strvec) {
  strvec_delete(strvec, strvec->line);
}

void strvec_delete(strvec_t *strvec, int line_index) {
  if (line_index > strvec->line) {
    printf("Error - line Out Of Bounds, Max line Is %d\n", strvec->line);
    exit(1);
  } else {
    for (int r = line_index; r < strvec->line; r++) {
      strcpy(strvec->data[r], strvec->data[r + 1]);
      strvec->len[r] = (int) strvec->len[r + 1];
    }
    strvec->line--;
  }
}

void strvec_wipeval(strvec_t *strvec, char *value) {
  for (int r = 0; r <= strvec->line; r++){
    if (strcmp(strvec->data[r], value) == 0){
        strvec_delete(strvec, r);
        r--;
      }
  }
}

int strvec_search(strvec_t *strvec, char *value) {
  for (int r = 0; r <= strvec->line; r++){
    if (strcmp(strvec->data[r], value) == 0) {
        return r;
    }
  }
  return -1;
}

int strvec_count(strvec_t *strvec, char *value) {
  int counter = 0;
  for (int r = 0; r <= strvec->line; r++) {
    if (strcmp(strvec->data[r], value) == 0) {
        counter++;
    }
  }
  return counter;
}

void strvec_swap_line(strvec_t *strvec, int line_source, int line_dest) {
  int temp_col;
  int vmax = vectormax(strvec->len, strvec->line);
  char *temp = malloc(vmax);

  strcpy(temp, strvec->data[line_source]);
  strcpy(strvec->data[line_source], strvec->data[line_dest]);
  strcpy(strvec->data[line_dest], temp);
  
  temp_col = strvec->len[line_source];
  strvec->len[line_source] = strvec->len[line_dest];
  strvec->len[line_dest] = temp_col;

  free(temp);
}

void strvec_sort_asc(strvec_t *strvec) {
  int i,j;
  int swapped = 0;

  for(i = 0; i < strvec->line; i++) { 
    swapped = 0;
    for(j = 0; j < strvec->line - i; j++) {
       if(strvec->data[j][0] > strvec->data[j+1][0]) {
        strvec_swap_line(strvec, j, j + 1);
          swapped = 1;
       }
    }
    if(swapped == 0) {
       break;
    }
  }
}

void strvec_sort_desc(strvec_t *strvec) {
  int i,j;
  int swapped = 0;

  for(i = 0; i < strvec->line; i++) { 
    swapped = 0;
    for(j = 0; j < strvec->line - i; j++) {
       if(strvec->data[j][0] < strvec->data[j+1][0]) {
        strvec_swap_line(strvec, j, j + 1);
          swapped = 1;
       }
    }
    if(swapped == 0) {
       break;
    }
  }
}

void strvec_print(strvec_t *strvec) {
  int i;
  i = (vectormax(strvec->len, strvec->line + 1) > 9) ? 9 : vectormax(strvec->len, strvec->line + 1) - 1;
  int vmax = vectormax(strvec->len, strvec->line + 1);
  int max_line = (strvec->line > 20) ? 10 : strvec->line;
  int max_line_bool = (strvec->line > 20) ? strvec->line : -100;

  // Header and Line
  printf("+");
  for (int s = 0; s < 12; s++) printf("-");
  printf("+");
  for (int i = 0; i < 50; i++) printf("-");
  printf("+\n|    line    |String Element:%35s|\n+", " ");
  for (int s = 0; s < 12; s++) printf("-");
  printf("+");
  for (int i = 0; i < 50; i++) printf("-");
  printf("+\n");

  // line Index & Content
  for (int r = 0; r <= max_line; r++) {
    printf("|%7d%5s|", r, " ");
    if (strcmp(strvec->data[r], "\0") == 0) {
      printf("%50s|\n", "(null)");
    } else {
      if (strlen(strvec->data[r]) >= 50) printf("%.47s...|\n", strvec->data[r]); else printf("%50s|\n", strvec->data[r]);
    }
  }
  if (strvec->line > 20) {
    for (int r = 0; r < 3; r++) {
      printf("|%7s%5s|", ".", " ");
      printf("%23s ...%23s|\n", " ", " ");
    }
  }
  for (int r = strvec->line - 3; r < max_line_bool; r++) {
    printf("|%7d%5s|", r, " ");
    if (strcmp(strvec->data[r], "\0") == 0) {
      printf("%50s|\n", "(null)");
    } else {
      if (strlen(strvec->data[r]) >= 50) printf("%.47s...|\n", strvec->data[r]); else printf("%50s|\n", strvec->data[r]);
    }
  }

  // Footer Line
  printf("+");
  for (int s = 0; s < 12; s++) printf("-");
  printf("+");
  for (int i = 0; i < 50; i++) printf("-");
  printf("+\n");
}

void strvec_compact(strvec_t *strvec) {
  int n_line = (strvec->line == 0) ? 1 : strvec->line;
  int n_col = (vectormax(strvec->len, strvec->line) == 0) ? 1 : vectormax(strvec->len, strvec->line);
  strvec->len = realloc(strvec->len, sizeof(int) * n_line);
  strvec->data = realloc(strvec->data, sizeof(char *) * n_line);
  for (int i = 0; i < n_line; i++)
    strvec->data[i] = realloc(strvec->data[i], sizeof(char) * n_col);
}

void strvec_free(strvec_t *strvec) {
  if (strvec->ready == 1) {
    strvec->ready = 0;
    for (int i = 0; i < strvec->line; i++) free(strvec->data[i]);
    free(strvec->data);
  }
}