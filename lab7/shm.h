#ifndef SHM_H
#define SHM_H

#include <sys/types.h>

#define SHM_KEY 0x1234       // Ключ для shmget (одинаковый для обеих программ)
#define MSG_SIZE 256         // Максимальный размер строки

typedef struct {
    pid_t writer_pid;        // PID процесса-отправителя
    int   writer_active;     // 1, если отправитель занял сегмент
    char  message[MSG_SIZE]; // Передаваемая строка (время + pid отправителя)
} shm_data_t;

#endif // SHM_H
