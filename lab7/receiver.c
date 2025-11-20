#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "shm.h"

int main(void) {
    // Пытаемся получить уже существующий сегмент
    int shmid = shmget(SHM_KEY, sizeof(shm_data_t), 0666);
    if (shmid == -1) {
        perror("shmget");
        fprintf(stderr, "Receiver: возможно, отправитель ещё не запущен.\n");
        exit(EXIT_FAILURE);
    }

    // Подключаем сегмент
    shm_data_t *shm = (shm_data_t *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    printf("Receiver: запущен. PID = %d\n", (int)getpid());
    printf("Receiver: разделяемая память shmid = %d\n", shmid);

    // Бесконечный цикл чтения
    while (1) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        printf("Receiver PID = %d, time = %s\n", (int)getpid(), time_str);

        if (shm->writer_active) {
            printf("  Принято из shared memory: \"%s\"\n", shm->message);
        } else {
            printf("  Отправитель сейчас не активен.\n");
        }

        printf("----------------------------------------------------\n");
        sleep(1);
    }

    // Теоретически сюда не дойдём
    shmdt(shm);
    return 0;
}
