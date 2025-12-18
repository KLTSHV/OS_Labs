#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "shm.h"

static int g_shmid = -1;
static shm_data_t *g_shm = NULL;
static int g_semid = -1;

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
#if defined(__linux__)
    struct seminfo *__buf;
#endif
};

static void sem_P(int semid) {
    struct sembuf op = {0, -1, 0}; // P (wait)
    while (semop(semid, &op, 1) == -1 && errno == EINTR) { }
}

static void sem_V(int semid) {
    struct sembuf op = {0, +1, 0}; // V (signal)
    while (semop(semid, &op, 1) == -1 && errno == EINTR) { }
}

static void cleanup_and_exit(int sig) {
    (void)sig;

    if (g_shm && g_shm != (void *)-1) {
        shmdt(g_shm);
        g_shm = NULL;
    }

    if (g_shmid != -1) {
        // удаляем сегмент shared memory
        shmctl(g_shmid, IPC_RMID, NULL);
        g_shmid = -1;
    }

    if (g_semid != -1) {
        // удаляем семафор
        semctl(g_semid, 0, IPC_RMID);
        g_semid = -1;
    }

    _exit(0);
}

int main(void) {
    // обработчики для корректной очистки ресурсов
    struct sigaction sa;
    sa.sa_handler = cleanup_and_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // shared memory
    g_shmid = shmget(SHM_KEY, sizeof(shm_data_t), IPC_CREAT | 0666);
    if (g_shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    g_shm = (shm_data_t *)shmat(g_shmid, NULL, 0);
    if (g_shm == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }


    int created = 0;
    g_semid = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (g_semid != -1) {
        created = 1;
    } else if (errno == EEXIST) {
        g_semid = semget(SEM_KEY, 1, 0666);
    }

    if (g_semid == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }

    if (created) {
        union semun u;
        u.val = 1; // бинарный семафор (mutex)
        if (semctl(g_semid, 0, SETVAL, u) == -1) {
            perror("semctl(SETVAL)");
            exit(EXIT_FAILURE);
        }
    }

    printf("Sender: PID=%d, shmid=%d, semid=%d\n", (int)getpid(), g_shmid, g_semid);

    while (1) {
        sleep(3);

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg), "Sender time=%s, sender PID=%d", time_str, (int)getpid());

        // критическая секция: запись строки в shared memory
        sem_P(g_semid);
        strncpy(g_shm->message, msg, MSG_SIZE - 1);
        g_shm->message[MSG_SIZE - 1] = '\0';
        sem_V(g_semid);

        printf("Sender: sent \"%s\"\n", msg);
    }
}