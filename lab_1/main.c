#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

int main() {
    int bad_fd = open("non_existent.txt", O_RDONLY);
    if (bad_fd == -1) {
        perror("Ошибка при открытии");
    }

    int out_fd = open("lab_routed.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd != -1) {
        dup2(out_fd, STDOUT_FILENO);
        close(out_fd);
        printf("Текст записан через перенаправленный дескриптор\n");
    }

    pid_t pid = fork();
    if (pid == 0) {
        execlp("echo", "echo", "Дочерний процесс запущен", NULL);
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    }
    return 0;
}

//Тестовый стенд для проверки отладки dtruss