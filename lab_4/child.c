#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>

struct shm {
    int done;
    char line[512];
};

int main(int argc, char *argv[]) {
    if (argc < 3) return 1;

    int file_fd = open(argv[1], O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_fd == -1) {
        perror("open");
        return 1;
    }

    int map_fd = open(argv[2], O_RDWR);
    if (map_fd == -1) {
        perror("open map");
        close(file_fd);
        return 1;
    }

    struct shm *addr = mmap(NULL, sizeof(struct shm), PROT_READ | PROT_WRITE, MAP_SHARED, map_fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        close(map_fd);
        close(file_fd);
        return 1;
    }

    sigset_t wait_sig;
    sigemptyset(&wait_sig);
    sigaddset(&wait_sig, SIGUSR1);
    sigprocmask(SIG_BLOCK, &wait_sig, NULL);

    int sig;
    while (sigwait(&wait_sig, &sig) == 0) {
        if (addr->done) break;

        int sum = 0;
        int num;
        int offset = 0;
        int read_bytes;

        while (sscanf(addr->line + offset, "%d%n", &num, &read_bytes) == 1) {
            sum += num;
            offset += read_bytes;
        }

        char result[128];
        int len = snprintf(result, sizeof(result), "Сумма: %d\n", sum);
        if (write(file_fd, result, len) == -1) {
            perror("write");
            break;
        }

        if (kill(getppid(), SIGUSR2) == -1) {
            perror("kill");
            break;
        }
    }

    munmap(addr, sizeof(struct shm));
    close(map_fd);
    close(file_fd);
    return 0;
}