#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <string.h>

#define SYSCALL_NUM 333 // Must match the number in custom_syscall.c

int main() {
    const char *test_str = "Hello from user space!";
    long ret = syscall(SYSCALL_NUM, test_str);
    if (ret == -1) {
        perror("syscall");
        return 1;
    }
    printf("Custom syscall returned length: %ld\n", ret);
    return 0;
}
