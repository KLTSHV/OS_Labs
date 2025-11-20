// file: fifo_reader.c
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
    // На всякий случай тоже пытаемся создать FIFO
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        if (errno != EEXIST) {
            perror("mkfifo");
            exit(EXIT_FAILURE);
        }
    }

    printf("Reader: opening FIFO for reading...\n");
    int fd = open(FIFO_PATH, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        close(fd);
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';

    // Ждём 10 секунд, чтобы время явно отличалось
    sleep(10);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

    printf("Reader: current time = %s\n", time_str);
    printf("Reader: message from FIFO: \"%s\"\n", buf);

    close(fd);
    return 0;
}
