#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#define FIFO_PATH "/tmp/my_fifo_example"

int main(void) {
    // Создаём FIFO, если её ещё нет
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }

    printf("Writer: opening FIFO for writing...\n");
    int fd = open(FIFO_PATH, O_WRONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    pid_t mypid = getpid();
    char msg[256];
    snprintf(msg, sizeof(msg),
             "Writer time = %s, writer PID = %d",
             time_str, (int)mypid);

    printf("Writer: sending message: \"%s\"\n", msg);

    ssize_t n = write(fd, msg, strlen(msg));
    if (n < 0) {
        perror("write");
        close(fd);
        exit(EXIT_FAILURE);
    }

    close(fd);
    return 0;
}
