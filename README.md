# Custom Syscall Kernel Module

## Description
This kernel module dynamically installs a custom system call at syscall number 548. The custom syscall accepts a user-space string, logs it in the kernel log, and returns the string length.

## Building the Module
To build the module, run:
```
make
```

## Loading the Module
To load the module safely, use:
```
sudo insmod custom_syscall.ko
```
Check the kernel log to confirm successful loading:
```
dmesg | tail
```

## Using the Custom Syscall
You can invoke the custom syscall from user space using syscall number 548. For example, in C:
```c
#include <unistd.h>
#include <sys/syscall.h>

#define CUSTOM_SYSCALL_NUM 548

long result = syscall(CUSTOM_SYSCALL_NUM, "Hello from user space");
```

## Unloading the Module
To unload the module safely, use:
```
sudo rmmod custom_syscall
```
Check the kernel log to confirm successful unloading:
```
dmesg | tail
```

## Notes
- Ensure no processes are using the custom syscall before unloading the module.
- Loading and unloading require root privileges.
- Kernel headers and build tools must be installed on your system.
