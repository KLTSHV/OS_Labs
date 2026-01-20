#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

volatile int stop = 0;
int counter = 0;
char channel[20];
pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
void handle_sigint(int sig){
    stop = 1;
    printf("stopping threads..\n");
}
void* writer_thread(void* arg){
    while(!stop){
        pthread_rwlock_wrlock(&rwlock);
        snprintf(channel, 20, "Entry %d", counter++);
        pthread_rwlock_unlock(&rwlock);
        usleep(800000);
    }
}

void* reader_thread(void* arg){
    long tid = (long)arg;
    while(!stop){
        pthread_rwlock_rdlock(&rwlock);
        printf("Reader %ld sees: %s\n", tid, channel);
        pthread_rwlock_unlock(&rwlock);
        usleep(500000);
    }
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