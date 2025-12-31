#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

volatile sig_atomic_t stop = 0;

char channel[20] = "Entry 0";
int counter = 0;

pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cv  = PTHREAD_COND_INITIALIZER;

unsigned long seq = 0;

void handle_sigint(int sig) {
    (void)sig;
    stop = 1;
    printf("stopping threads..\n");

    pthread_mutex_lock(&mtx);
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);
}

void* writer_thread(void* arg) {
    (void)arg;

    while (!stop) {
        pthread_mutex_lock(&mtx);

        snprintf(channel, sizeof(channel), "Entry %d", counter++);
        seq++;

        pthread_cond_broadcast(&cv);

        pthread_mutex_unlock(&mtx);

        usleep(800000);
    }

    return NULL;
}

void* reader_thread(void* arg) {
    long tid = (long)arg;
    unsigned long last_seen = 0;

    while (!stop) {
        pthread_mutex_lock(&mtx);

        while (!stop && seq == last_seen) {
            pthread_cond_wait(&cv, &mtx);
        }

        if (stop) {
            pthread_mutex_unlock(&mtx);
            break;
        }

        char local[20];
        strncpy(local, channel, sizeof(local));
        local[sizeof(local) - 1] = '\0';
        last_seen = seq;

        pthread_mutex_unlock(&mtx);

        printf("Reader %ld sees: %s\n", tid, local);
        usleep(500000);
    }

    return NULL;
}

int main() {
    pthread_t readers[10];
    pthread_t writer;

    signal(SIGINT, handle_sigint);

    for (long i = 0; i < 10; i++) {
        pthread_create(&readers[i], NULL, reader_thread, (void*)i);
    }
    pthread_create(&writer, NULL, writer_thread, NULL);

    for (int i = 0; i < 10; i++) {
        pthread_join(readers[i], NULL);
    }
    pthread_join(writer, NULL);

    return 0;
}