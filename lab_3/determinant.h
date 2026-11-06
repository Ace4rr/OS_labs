#ifndef DETERMINANT_H
#define DETERMINANT_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef struct{
    size_t size;
    double *data;
} Matrix;

typedef struct{
    const Matrix *matrix;
    size_t next_col;
    double total_det;
    pthread_mutex_t task_mutex;
    pthread_mutex_t result_mutex;
} SharedContext;

typedef struct{
    int thread_index;
    SharedContext *ctx;
} ThreadArg;

int matrix_init(Matrix *mat, size_t size);
void matrix_free(Matrix *mat);
void matrix_fill_random(Matrix *mat, unsigned int seed);
int matrix_read_stream(Matrix *mat, void *stream);
void matrix_print(const Matrix *mat);

double determinant_sequential(const Matrix *mat);
int determinant_parallel(const Matrix *mat, size_t max_threads, double 
*out_det);
uint64_t get_current_tid(void);

#endif
