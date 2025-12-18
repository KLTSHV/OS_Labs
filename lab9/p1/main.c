#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>

#define BUF_SIZE 128

static char g_buf[BUF_SIZE];   // общий массив символов
static sem_t g_sem;            // семафор для синхронизации доступа к массиву
static volatile int g_run = 1; // флаг работы

void *writer_thread(void *arg) {
    (void)arg;
    unsigned long counter = 0;

    while (g_run) {
        sleep(1);
        // критическая секция: запись в общий массив
        sem_wait(&g_sem);
        counter++;
        snprintf(g_buf, sizeof(g_buf), "%lu", counter);
        sem_post(&g_sem);
    }
    return NULL;
}

void *reader_thread(void *arg) {
    (void)arg;
    pthread_t tid = pthread_self();

    while (g_run) {
        char local[BUF_SIZE];

        // критическая секция: чтение общего массива
        sem_wait(&g_sem);
        strncpy(local, g_buf, sizeof(local) - 1);
        local[sizeof(local) - 1] = '\0';

        printf("Reader tid=%lu, buf=\"%s\"\n", (unsigned long)tid, local);
    }
    return NULL;
}

int main(void) {
    // инициализируем общий массив
    strncpy(g_buf, "0", sizeof(g_buf) - 1);
    g_buf[sizeof(g_buf) - 1] = '\0';

    // инициализация семафора: pshared=0 (только внутри процесса), начальное значение 1
    if (sem_init(&g_sem, 0, 1) != 0) {
        perror("sem_init");
        return 1;
    }

    pthread_t w, r;
    if (pthread_create(&w, NULL, writer_thread, NULL) != 0) {
        perror("pthread_create(writer)");
        return 1;
    }
    if (pthread_create(&r, NULL, reader_thread, NULL) != 0) {
        perror("pthread_create(reader)");
        return 1;
    }
    // пусть работает бесконечно; при желании можно остановить по Ctrl+C (оставлено как есть)
    pthread_join(w, NULL);
    pthread_join(r, NULL);

    sem_destroy(&g_sem);
    return 0;
}