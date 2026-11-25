#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <string.h>

struct shm {
    int done;
    char line[512];
};
int main(){
    char filename[256];
    printf("Enter filename:");
    if (scanf("%255s", filename) != 1) return 1;
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    int fd = open("data.bin", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        return 1;
    }
    if (ftruncate(fd, sizeof(struct shm)) == -1) {
        perror("ftruncate");
        close(fd);
        return 1;
    }
    struct shm *addr = mmap(NULL, sizeof(struct shm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    sigset_t mask, ack;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
        perror("sigprocmask");
        return 1;
    }
    sigemptyset(&ack);
    sigaddset(&ack, SIGUSR2);
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        close(fd);
        execl("./child", "child", filename, "data.bin", NULL);
        perror("execl failed");
        exit(1);
    }
    char line[512];
    int sig;
    printf("Enter number:\n");
    while (fgets(line, sizeof(line), stdin) != NULL) {
        addr->done = 0;
        strcpy(addr->line, line);
        if (kill(pid, SIGUSR1) == -1) {
            perror("kill");
            break;
        }
        if (sigwait(&ack, &sig) != 0) break;
    }
    addr->done = 1;
    kill(pid, SIGUSR1);
    waitpid(pid, NULL, 0); // убрали зомби
    munmap(addr, sizeof(struct shm));
    close(fd);
    unlink("data.bin");
    return 0;
}