#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>


char channel[16];
int counter = 0;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
volatile int stop = 0;
void handle_sigint(int sig){
    stop = 1;
    printf("stopping threads..\n");
}

void* writer_thread(void* arg){
    while(!stop){
        pthread_mutex_lock(&mutex);
        snprintf(channel, 16, "Entry %d", counter++);
        pthread_mutex_unlock(&mutex);
        usleep(800000);
    }
    return NULL;

}
void* reader_thread(void* arg){
    long tid = (long)arg;
    while(!stop){
        pthread_mutex_lock(&mutex);
        printf("Reader %ld sees: %s\n", tid, channel);
        pthread_mutex_unlock(&mutex);
        usleep(500000);
    }
    return NULL;
}

int main(){
    pthread_t readers[10];
    pthread_t writer;
    signal(SIGINT, handle_sigint);
    for(long i = 0; i < 10; i++){
        pthread_create(&readers[i], NULL, reader_thread, (void*)i);
    }
    pthread_create(&writer, NULL, writer_thread, NULL);
    for(int i = 0; i < 10; i++){
        pthread_join(readers[i], NULL);
    }
    pthread_join(writer, NULL);
    return 0;
}