// pmms.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>

#define NUM_CHILDREN 3

pid_t child_pids[NUM_CHILDREN];
int paused[NUM_CHILDREN] = {0};

// Log function
void log_event(const char *event) {
    FILE *log = fopen("pmms.log", "a");
    if (log) {
        time_t now = time(NULL);
        fprintf(log, "[%s] %s\n", strtok(ctime(&now), "\n"), event);
        fclose(log);
    }
}

// Signal handling for children
volatile sig_atomic_t is_paused = 0;

void handle_sigusr1(int sig) {
    is_paused = !is_paused;
}

void handle_sigterm(int sig) {
    printf("[Child %d] Terminating...\n", getpid());
    exit(0);
}

// Child process behavior
void child_process() {
    signal(SIGUSR1, handle_sigusr1);
    signal(SIGTERM, handle_sigterm);

    while (1) {
        if (!is_paused) {
            printf("[Child %d] Active ...\n", getpid());
            fflush(stdout);
        }
        sleep(3);
    }
}

// Parent signal handler
void handle_sigint(int sig) {
    printf("\n[Parent] Terminating all children...\n");
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGTERM);
        }
    }

    while (wait(NULL) > 0);
    log_event("All children terminated. Parent exiting.");
    printf("[Parent] All children terminated. Exiting now.\n");
    exit(0);
}

int main() {
    signal(SIGINT, handle_sigint);

    printf("Parent PID: %d\n", getpid());
    log_event("Parent process started.");

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // In child
            child_process();
        } else if (pid > 0) {
            // In parent
            child_pids[i] = pid;
            char msg[100];
            sprintf(msg, "Child process created with PID %d", pid);
            log_event(msg);
        } else {
            perror("fork");
            exit(1);
        }
    }

    while (1) {
        pause();
    }

    return 0;
}

