#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>

int main(void) {
    int fd[2];// fd[0] - чтение, fd[1] - запись
    pid_t pid;

    if (pipe(fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Дочерний процесс
        close(fd[1]); // не будем писать

        char buf[256];
        ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
        if (n < 0) {
            perror("read");
            close(fd[0]);
            exit(EXIT_FAILURE);
        }
        buf[n] = '\0';

        // Ждём 5 секунд, чтобы время отличалось
        sleep(5);

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        printf("Child: current time = %s\n", time_str);
        printf("Child: received from pipe: \"%s\"\n", buf);

        close(fd[0]);
        exit(EXIT_SUCCESS);
    } else {
        // Родитель
        close(fd[0]); // не будем читать

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        pid_t mypid = getpid();

        char msg[256];
        snprintf(msg, sizeof(msg),
                 "Parent time = %s, parent PID = %d",
                 time_str, (int)mypid);

        ssize_t n = write(fd[1], msg, strlen(msg));
        if (n < 0) {
            perror("write");
            close(fd[1]);
            exit(EXIT_FAILURE);
        }

        close(fd[1]);
        wait(NULL); // ждём ребёнка
    }

    return 0;
}
