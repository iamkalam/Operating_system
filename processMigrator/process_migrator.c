#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

void checkpoint_process(pid_t pid) {
    char command[256];
    const char *checkpoint_dir = "/tmp/checkpoint";

    // Create the checkpoint directory if it doesn't exist
    if (mkdir(checkpoint_dir, 0755) == -1 && access(checkpoint_dir, F_OK) != 0) {
        perror("Failed to create checkpoint directory");
        return;
    }

    snprintf(command, sizeof(command), "criu dump -t %d -D %s --shell-job --unprivileged", pid, checkpoint_dir);
    int ret = system(command);
    if (ret != 0) {
        printf("Checkpointing failed. Ensure CRIU is configured correctly and the process is checkpointable.\n");
    }
}

void restore_process() {
    char command[256];
    const char *checkpoint_dir = "/tmp/checkpoint";

    snprintf(command, sizeof(command), "criu restore -D %s --shell-job --unprivileged", checkpoint_dir);
    int ret = system(command);
    if (ret != 0) {
        printf("Restoring failed. Ensure CRIU is configured correctly and the checkpoint exists.\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <checkpoint|restore> [pid]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "checkpoint") == 0) {
        if (argc < 3) {
            printf("Please provide a PID to checkpoint.\n");
            return 1;
        }
        pid_t pid = atoi(argv[2]);
        checkpoint_process(pid);
    } else if (strcmp(argv[1], "restore") == 0) {
        restore_process();
    } else {
        printf("Invalid command. Use 'checkpoint' or 'restore'.\n");
    }

    return 0;
}