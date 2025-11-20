#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "shm.h"

static shm_data_t *g_shm = NULL; // указатель на общую память для обработчика сигналов

void cleanup_and_exit(int sig) {
    if (g_shm) {
        g_shm->writer_active = 0; // снимаем флаг активности
    }
    _exit(0); // быстрая посадка без лишних действий
}

int main(void) {
    // Устанавливаем обработчики для корректного снятия флага при завершении
    struct sigaction sa;
    sa.sa_handler = cleanup_and_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Получаем или создаём сегмент разделяемой памяти
    int shmid = shmget(SHM_KEY, sizeof(shm_data_t), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // Подключаем сегмент
    g_shm = (shm_data_t *)shmat(shmid, NULL, 0);
    if (g_shm == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Проверяем, не работает ли уже отправитель
    if (g_shm->writer_active) {
        pid_t existing_pid = g_shm->writer_pid;
        if (existing_pid > 0 && kill(existing_pid, 0) == 0) {
            // Процесс с таким PID жив
            fprintf(stderr,
                    "Sender: уже запущен процесс-отправитель с PID = %d\n",
                    (int)existing_pid);
            shmdt(g_shm);
            return 1;
        } else {
            // PID некорректен или процесс уже мёртв — можем захватить
            fprintf(stderr,
                    "Sender: найден устаревший PID (%d), перехватываем роль отправителя.\n",
                    (int)existing_pid);
        }
    }

    // Захватываем роль отправителя
    g_shm->writer_pid = getpid();
    g_shm->writer_active = 1;

    printf("Sender: запущен. PID = %d\n", (int)getpid());
    printf("Sender: разделяемая память shmid = %d\n", shmid);
    printf("Sender: нажмите Ctrl+C для выхода.\n");

    // Основной цикл: раз в секунду обновляем сообщение
    while (1) {
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);

        char time_str[64];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        // Формируем строку
        char msg[MSG_SIZE];
        snprintf(msg, sizeof(msg),
                 "Sender time = %s, sender PID = %d",
                 time_str, (int)getpid());

        // Записываем в разделяемую память
        strncpy(g_shm->message, msg, MSG_SIZE - 1);
        g_shm->message[MSG_SIZE - 1] = '\0';

        // на всякий случай обновляем PID и флаг
        g_shm->writer_pid = getpid();
        g_shm->writer_active = 1;

        // Просто для информации в консоли отправителя:
        printf("Sender: записано в shared memory: \"%s\"\n", g_shm->message);

        sleep(1);
    }

    // Теоретически сюда не дойдём, но энивэй:
    g_shm->writer_active = 0;
    shmdt(g_shm);
    return 0;
}
