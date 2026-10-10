#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[]){
    if (argc < 2) {
return 1;}
    int file_fd = open(argv[1], O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (file_fd == -1){
        perror("open");
        return 1;}

    char line[512];
    while (fgets(line, sizeof(line), stdin) != NULL){
        int sum = 0;
        int num;
        int offset = 0;
        int read_bytes;

        while (sscanf(line + offset, "%d%n", &num, &read_bytes) == 1) {
            sum += num;
            offset += read_bytes;}

        char result[128];
        int len = snprintf(result, sizeof(result), "Сумма: %d\n", sum);
        write(file_fd, result, len);
    }
    close(file_fd);
}
