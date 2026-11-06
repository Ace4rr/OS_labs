#include "determinant.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <pthread.h>
#else
#include <sys/syscall.h>
#endif

uint64_t get_current_tid(void) {
#ifdef __APPLE__
    uint64_t tid = 0;
    if (pthread_threadid_np(NULL, &tid) != 0) {
        return 0;
    }
    return tid;
#else
    return (uint64_t)syscall(SYS_gettid);
#endif
}

int matrix_init(Matrix *mat, size_t size) {
    if (mat == NULL || size == 0) {
        errno = EINVAL;
        return -1;
    }
    mat->size = size;
    mat->data = (double *)calloc(size * size, sizeof(double));
    if (mat->data == NULL) {
        perror("Ошибка выделения памяти (calloc)");
        return -1;
    }
    return 0;
}

void matrix_free(Matrix *mat) {
    if (mat != NULL && mat->data != NULL) {
        free(mat->data);
        mat->data = NULL;
        mat->size = 0;
    }
}

void matrix_fill_random(Matrix *mat, unsigned int seed) {
    srand(seed);
    size_t total = mat->size * mat->size;
    for (size_t i = 0; i < total; ++i) {
        mat->data[i] = (double)((rand() % 11) - 5);
    }
}

int matrix_read_stream(Matrix *mat, void *stream) {
    FILE *fp = (FILE *)stream;
    size_t total = mat->size * mat->size;
    for (size_t i = 0; i < total; ++i) {
        if (fscanf(fp, "%lf", &mat->data[i]) != 1) {
            fprintf(stderr, "Ошибка чтения элемента матрицы с индексом %zu\n", i);
            return -1;
        }
    }
    return 0;
}

void matrix_print(const Matrix *mat) {
    for (size_t r = 0; r < mat->size; ++r) {
        for (size_t c = 0; c < mat->size; ++c) {
            printf("%6.1f ", mat->data[r * mat->size + c]);
        }
        printf("\n");
    }
}

static int build_submatrix(const Matrix *src, size_t skip_col, Matrix *sub) {
    size_t n = src->size;
    if (matrix_init(sub, n - 1) != 0) {
        return -1;
    }
    for (size_t r = 1; r < n; ++r) {
        size_t sub_c = 0;
        for (size_t c = 0; c < n; ++c) {
            if (c == skip_col) {
                continue;
            }
            sub->data[(r - 1) * (n - 1) + sub_c] = src->data[r * n + c];
            ++sub_c;
        }
    }
    return 0;
}

double determinant_sequential(const Matrix *mat) {
    size_t n = mat->size;
    if (n == 1) {
        return mat->data[0];
    }
    if (n == 2) {
        return mat->data[0] * mat->data[3] - mat->data[1] * mat->data[2];
    }

    double det = 0.0;
    Matrix sub;
    if (matrix_init(&sub, n - 1) != 0) {
        exit(EXIT_FAILURE);
    }

    for (size_t col = 0; col < n; ++col) {
        double coeff = mat->data[col];
        if (coeff == 0.0) {
            continue;
        }
        for (size_t r = 1; r < n; ++r) {
            size_t sub_c = 0;
            for (size_t c = 0; c < n; ++c) {
                if (c == col) {
                    continue;
                }
                sub.data[(r - 1) * (n - 1) + sub_c] = mat->data[r * n + c];
                ++sub_c;
            }
        }
        double sign = (col % 2 == 0) ? 1.0 : -1.0;
        det += sign * coeff * determinant_sequential(&sub);
    }

    matrix_free(&sub);
    return det;
}

static void *worker_routine(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    SharedContext *ctx = targ->ctx;
    const Matrix *mat = ctx->matrix;
    size_t n = mat->size;

    pid_t pid = getpid();
    uint64_t tid = get_current_tid();
    double local_sum = 0.0;

    while (1) {
        int rc = pthread_mutex_lock(&ctx->task_mutex);
        if (rc != 0) {
            errno = rc;
            perror("pthread_mutex_lock (task)");
            pthread_exit(NULL);
        }

        size_t col = ctx->next_col;
        if (col < n) {
            ctx->next_col++;
        }

        rc = pthread_mutex_unlock(&ctx->task_mutex);
        if (rc != 0) {
            errno = rc;
            perror("pthread_mutex_unlock (task)");
            pthread_exit(NULL);
        }

        if (col >= n) {
            break;
        }

        double coeff = mat->data[col];
        double cofactor_val = 0.0;

        if (coeff != 0.0) {
            Matrix sub;
            if (build_submatrix(mat, col, &sub) != 0) {
                pthread_exit(NULL);
            }
            double sign = (col % 2 == 0) ? 1.0 : -1.0;
            cofactor_val = sign * coeff * determinant_sequential(&sub);
            matrix_free(&sub);
        }

        local_sum += cofactor_val;
        printf("[PID: %d | TID: 0x%llx | Поток #%d] Обработан столбец %zu, вклад = %.2f\n",
               (int)pid, (unsigned long long)tid, targ->thread_index, col, cofactor_val);
    }

    int rc = pthread_mutex_lock(&ctx->result_mutex);
    if (rc != 0) {
        errno = rc;
        perror("pthread_mutex_lock (result)");
        pthread_exit(NULL);
    }

    ctx->total_det += local_sum;

    rc = pthread_mutex_unlock(&ctx->result_mutex);
    if (rc != 0) {
        errno = rc;
        perror("pthread_mutex_unlock (result)");
        pthread_exit(NULL);
    }

    return NULL;
}

int determinant_parallel(const Matrix *mat, size_t max_threads, double *out_det) {
    if (mat == NULL || out_det == NULL || max_threads == 0) {
        errno = EINVAL;
        return -1;
    }

    if (mat->size <= 2) {
        *out_det = determinant_sequential(mat);
        return 0;
    }

    size_t active_threads = (max_threads < mat->size) ? max_threads : mat->size;

    SharedContext ctx;
    ctx.matrix = mat;
    ctx.next_col = 0;
    ctx.total_det = 0.0;

    int rc = pthread_mutex_init(&ctx.task_mutex, NULL);
    if (rc != 0) {
        errno = rc;
        perror("pthread_mutex_init (task)");
        return -1;
    }

    rc = pthread_mutex_init(&ctx.result_mutex, NULL);
    if (rc != 0) {
        errno = rc;
        perror("pthread_mutex_init (result)");
        pthread_mutex_destroy(&ctx.task_mutex);
        return -1;
    }

    pthread_t *threads = (pthread_t *)calloc(active_threads, sizeof(pthread_t));
    ThreadArg *args = (ThreadArg *)calloc(active_threads, sizeof(ThreadArg));
    if (threads == NULL || args == NULL) {
        perror("Ошибка выделения памяти под потоки");
        free(threads);
        free(args);
        pthread_mutex_destroy(&ctx.task_mutex);
        pthread_mutex_destroy(&ctx.result_mutex);
        return -1;
    }

    size_t created_count = 0;
    for (size_t i = 0; i < active_threads; ++i) {
        args[i].thread_index = (int)i;
        args[i].ctx = &ctx;
        rc = pthread_create(&threads[i], NULL, worker_routine, &args[i]);
        if (rc != 0) {
            errno = rc;
            perror("pthread_create");
            break;
        }
        created_count++;
    }

    for (size_t i = 0; i < created_count; ++i) {
        rc = pthread_join(threads[i], NULL);
        if (rc != 0) {
            errno = rc;
            perror("pthread_join");
        }
    }

    *out_det = ctx.total_det;

    free(threads);
    free(args);
    pthread_mutex_destroy(&ctx.task_mutex);
    pthread_mutex_destroy(&ctx.result_mutex);

    return (created_count == active_threads) ? 0 : -1;
}
