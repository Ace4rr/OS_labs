#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(){
    char filename[256];
    printf("Enter filename:");
    if (scanf("%255s", filename) != 1) return 1;
    int c;
    while ((c = getchar()) != '\n' && c != EOF);

    int pipe1[2];
    if (pipe(pipe1) == -1) {
        perror("pipe");
        return 1;}

    pid_t pid = fork();
    if (pid == -1){
        perror("fork");
        return 1;}

    if (pid == 0){
        close(pipe1[1]);//закрываем пайп на запись ребенка 
        dup2(pipe1[0], STDIN_FILENO);
        close(pipe1[0]);
        execl("./child", "child", filename, NULL);
        perror("execl failed");
        exit(1);
    } else{
        close(pipe1[0]);

        char line[512];
        printf("Enter number:\n");
        while (fgets(line, sizeof(line), stdin) != NULL) {
            write(pipe1[1], line, strlen(line));
        }
        close(pipe1[1]);
        waitpid(pid, NULL, 0);//удплили зомби
}}
