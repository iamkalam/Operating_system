#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ksls");
MODULE_DESCRIPTION("Kernel module to add a custom system call");

// Using a high number to avoid conflicts with existing syscalls
#define SYSCALL_NUM 548  

// Prototype for custom_syscall
asmlinkage long custom_syscall(const char __user *user_str);

asmlinkage long custom_syscall(const char __user *user_str) {
    char buf[256];
    long len;

    if (!user_str)
        return -EINVAL;

    // Safe copy from user space with length limit
    len = strncpy_from_user(buf, user_str, sizeof(buf) - 1);
    
    if (len < 0)
        return -EFAULT;
    
    // Ensure null termination
    buf[len] = '\0';

    printk(KERN_INFO "custom_syscall: received string '%s' of length %ld\n", buf, len);

    return len;
}

// Dynamic lookup for syscall table
static unsigned long **syscall_table = NULL;

// Function to find syscall table dynamically
static unsigned long lookup_name(const char *name) {
    struct kprobe kp = {
        .symbol_name = name
    };
    unsigned long ret;
    
    if (register_kprobe(&kp) < 0) return 0;
    ret = (unsigned long)kp.addr;
    unregister_kprobe(&kp);
    return ret;
}

static asmlinkage long (*original_syscall)(const char __user *);

// Corrected function to disable write protection
static void disable_write_protection(void) {
    unsigned long cr0;
    preempt_disable();
    barrier();
    cr0 = read_cr0();
    cr0 &= ~0x00010000; // Clear WP bit (bit 16)
    write_cr0(cr0);
    barrier();
    preempt_enable();
}

// Corrected function to enable write protection
static void enable_write_protection(void) {
    unsigned long cr0;
    preempt_disable();
    barrier();
    cr0 = read_cr0();
    cr0 |= 0x00010000; // Set WP bit (bit 16)
    write_cr0(cr0);
    barrier();
    preempt_enable();
}

static int __init custom_syscall_init(void) {
    printk(KERN_INFO "custom_syscall: Loading module - start\n");

    // Find syscall table dynamically
    syscall_table = (unsigned long **)lookup_name("sys_call_table");
    
    if (!syscall_table) {
        printk(KERN_ERR "custom_syscall: Could not find sys_call_table\n");
        return -1;
    }

    printk(KERN_INFO "custom_syscall: Found sys_call_table at %px\n", syscall_table);

    // Save original syscall
    original_syscall = (void *)syscall_table[SYSCALL_NUM];
    
    // Check if the slot is already in use
    if (original_syscall != NULL && 
        (unsigned long)original_syscall != 0 && 
        (unsigned long)original_syscall != -1) {
        printk(KERN_ERR "custom_syscall: Syscall %d is already in use\n", SYSCALL_NUM);
        return -1;
    }

    // Disable write protection
    disable_write_protection();
    
    // Install our syscall
    syscall_table[SYSCALL_NUM] = (unsigned long *)custom_syscall;
    
    // Enable write protection
    enable_write_protection();

    printk(KERN_INFO "custom_syscall: Installed at syscall number %d\n", SYSCALL_NUM);
    printk(KERN_INFO "custom_syscall: Loading module - end\n");
    return 0;
}

static void __exit custom_syscall_exit(void) {
    if (!syscall_table) {
        printk(KERN_ERR "custom_syscall: syscall_table is NULL in exit\n");
        return;
    }

    printk(KERN_INFO "custom_syscall: Unloading module\n");

    // Disable write protection
    disable_write_protection();
    
    // Restore original syscall
    syscall_table[SYSCALL_NUM] = (unsigned long *)original_syscall;
    
    // Enable write protection
    enable_write_protection();

    printk(KERN_INFO "custom_syscall: Restored original syscall\n");
}

module_init(custom_syscall_init);
module_exit(custom_syscall_exit);
