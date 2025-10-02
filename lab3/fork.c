#include <_string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
void at_exit_parent(void){
    printf("[PARENT atexit] PID=%d finishing (PPID=%d)\n", getpid(), getppid());
}
void at_exit_child(void){
    printf("[CHILD atexit] PID=%d finishing (PPID=%d)\n", getpid, getppid());
}
void sigint_handler(int signum){
    printf("[signal()] Caught signal %d (%s) in PID=%d\n",
    signum, strsignal(signum), getpid());
    fflush(stdout);

}
void sigterm_handler(int signum, siginfo_t *info, void *ucontext){
    const char *desc = strsignal(signum);
    pid_t sender = info ? info->si_pid : -1;
    printf("[sigaction()] Caught signal %d (%s) in PID=%d\n",
        signum, strsignal(signum), getpid());
    
    fflush(stdout);
}
int install_handlers(){
    if(signal(SIGINT, sigint_handler) == SIG_ERR){
        perror("signal(SIGINT) failed");
        return -1;
    }
    struct sigaction sa;
    memset (&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigterm_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction failed");
        return -1;
    }
    return 0;
}
int main(void){
    if(install_handlers() == -1){
        fprintf(stderr, "Failed to install signal handlers");
        return 1;
    }
    printf("[MAIN] PID=%d (PPID=%d). About to fork.. \n", getpid(), getppid());
    fflush(stdout);

    pid_t pid = fork();
    if(pid < 0){
        perror("fork failed");
        return 1;
    } else if (pid == 0){
        if(atexit(at_exit_child) != 0){
            perror("atexit(child) failed");
        }
        printf("[CHILD] Hello! PID=%d, PPID=%d", getpid(), getppid());
        fflush(stdout);
        //Спим чтобы моджно было послать kill -INT/-TERM
        sleep(2);

        printf("[CHILD] Exiting with code %d\n", 42);
        fflush(stdout);
        _exit(42);
    } else {
        //Родительский процесс
        if(atexit(at_exit_parent)!=0){
            perror("atexit(parent) failed");
        }
        printf("[PARENT ] Hello! PID=%d, Child PID=%d, PPID=%d", getpid(), pid, getppid());
        fflush(stdout);

        int status = 0;
        pid_t w = waitpid(pid, &status, 0);
        if (w==-1){
            perror("waitpid failed");
            return 1;
        }
        if(WIFEXITED(status)){
            int code = WEXITSTATUS(status);
            printf("[PARENT] Child %d exited normally with code %d\n", pid, code);
        } else if (WIFSIGNALED(status)){
            int sig = WTERMSIG(status);
            printf("[PARENT] Child %d terminated by signal %d (%s)\n", pid, sig, strsignal(sig));
        } else {
            printf("[PARENT] Child %d ended in an unexpected way (status=0x%x)\n", pid, status);
        }
        fflush(stdout);
    }
    return 0;
}