#include "determinant.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "Использование: %s -t <макс_потоков> [-n <размер_матрицы>] [-f <файл>] [-s <seed>]\n", prog);
}

static double get_elapsed_seconds(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
}

int main(int argc, char *argv[]) {
    size_t max_threads = 0;
    size_t matrix_size = 10;
    unsigned int seed = 42;
    const char *filename = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "t:n:f:s:h")) != -1) {
        char *endptr = NULL;
        errno = 0;
        switch (opt) {
            case 't': {
                long val = strtol(optarg, &endptr, 10);
                if (errno != 0 || *endptr != '\0' || val <= 0) {
                    fprintf(stderr, "Ошибка: некорректное число потоков (-t)\n");
                    return EXIT_FAILURE;
                }
                max_threads = (size_t)val;
                break;
            }
            case 'n': {
                long val = strtol(optarg, &endptr, 10);
                if (errno != 0 || *endptr != '\0' || val <= 0 || val > 15) {
                    fprintf(stderr, "Ошибка: размер матрицы (-n) должен быть от 1 до 15\n");
                    return EXIT_FAILURE;
                }
                matrix_size = (size_t)val;
                break;
            }
            case 'f':
                filename = optarg;
                break;
            case 's': {
                long val = strtol(optarg, &endptr, 10);
                if (errno != 0 || *endptr != '\0' || val < 0) {
                    fprintf(stderr, "Ошибка: некорректный seed (-s)\n");
                    return EXIT_FAILURE;
                }
                seed = (unsigned int)val;
                break;
            }
            case 'h':
            default:
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (max_threads == 0) {
        fprintf(stderr, "Ошибка: ключ -t <макс_потоков> обязателен.\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    Matrix mat;
    if (filename != NULL) {
        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
            perror("Ошибка при открытии файла матрицы");
            return EXIT_FAILURE;
        }
        if (fscanf(fp, "%zu", &matrix_size) != 1 || matrix_size == 0) {
            fprintf(stderr, "Ошибка чтения размера матрицы из файла\n");
            fclose(fp);
            return EXIT_FAILURE;
        }
        if (matrix_init(&mat, matrix_size) != 0) {
            fclose(fp);
            return EXIT_FAILURE;
        }
        if (matrix_read_stream(&mat, fp) != 0) {
            matrix_free(&mat);
            fclose(fp);
            return EXIT_FAILURE;
        }
        if (fclose(fp) != 0) {
            perror("Ошибка закрытия файла");
        }
    } else {
        if (matrix_init(&mat, matrix_size) != 0) {
            return EXIT_FAILURE;
        }
        matrix_fill_random(&mat, seed);
    }

    printf("=== Параметры запуска ===\n");
    printf("Основной процесс PID: %d | Главный поток TID: 0x%llx\n",
           (int)getpid(), (unsigned long long)get_current_tid());
    printf("Размер матрицы: %zu x %zu | Лимит потоков (-t): %zu\n\n",
           mat.size, mat.size, max_threads);

    if (mat.size <= 6) {
        printf("Исходная матрица:\n");
        matrix_print(&mat);
        printf("\n");
    }

    struct timespec start_time, end_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) == -1) {
        perror("clock_gettime");
        matrix_free(&mat);
        return EXIT_FAILURE;
    }

    double det = 0.0;
    if (determinant_parallel(&mat, max_threads, &det) != 0) {
        matrix_free(&mat);
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end_time) == -1) {
        perror("clock_gettime");
        matrix_free(&mat);
        return EXIT_FAILURE;
    }

    double elapsed = get_elapsed_seconds(start_time, end_time);
    printf("\n=== Результаты ===\n");
    printf("Детерминант: %.2f\n", det);
    printf("Время выполнения: %.6f сек.\n", elapsed);

    matrix_free(&mat);
    return EXIT_SUCCESS;
}
