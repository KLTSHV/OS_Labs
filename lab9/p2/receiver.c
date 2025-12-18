#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "shm.h"

static void sem_P(int semid) {
    struct sembuf op = {0, -1, 0};
    while (semop(semid, &op, 1) == -1 && errno == EINTR) { }
}

static void sem_V(int semid) {
    struct sembuf op = {0, +1, 0};
    while (semop(semid, &op, 1) == -1 && errno == EINTR) { }
}

int main(void) {
    // получаем существующий сегмент shmem
    int shmid = shmget(SHM_KEY, sizeof(shm_data_t), 0666);
    if (shmid == -1) {
        perror("shmget");
        fprintf(stderr, "Receiver: возможно, отправитель ещё не запущен.\n");
        exit(EXIT_FAILURE);
    }

    shm_data_t *shm = (shm_data_t *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // получаем существующий семафор
    int semid = semget(SEM_KEY, 1, 0666);
    if (semid == -1) {
        perror("semget");
        fprintf(stderr, "Receiver: возможно, отправитель ещё не создал семафор.\n");
        exit(EXIT_FAILURE);
    }

    printf("Receiver: PID=%d, shmid=%d, semid=%d\n", (int)getpid(), shmid, semid);

    while (1) {

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        char local[MSG_SIZE];

        // критическая секция: чтение строки (и вывод синхронизируем тем же семафором)
        sem_P(semid);
        strncpy(local, shm->message, sizeof(local) - 1);
        local[sizeof(local) - 1] = '\0';

        printf("Receiver time=%s, receiver PID=%d\n", time_str, (int)getpid());
        printf("  Received: \"%s\"\n", local);
        printf("----------------------------------------------------\n");
        sem_V(semid);
    }

    // теоретически сюда не дойдём
    shmdt(shm);
    return 0;
}