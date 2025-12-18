#pragma once
#include <sys/types.h>

#define SHM_KEY 0x12345
#define SEM_KEY 0x54321

#define MSG_SIZE 256

typedef struct {
    char message[MSG_SIZE];
} shm_data_t;