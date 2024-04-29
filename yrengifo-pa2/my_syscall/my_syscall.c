#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>

SYSCALL_DEFINE1(yrengifo_syscall, char __user *, user_msg) {
    char kernel[33] = {0};
    int count = 0;
    char letter = 'Y'; 
    int len;
    if (user_msg == NULL)
        return -1;

    len = strnlen_user(user_msg, 33);

    if (len == 0 || len > 33) //  includes '\0'
        return -1;
    
    if(copy_from_user(kernel,user_msg,len))
       return -EFAULT;

    printk(KERN_INFO "before: %s\n", kernel);

    for (int i = 0; kernel[i] != '\0'; i++) {
        if (kernel[i] == 'a' || kernel[i] == 'e' || kernel[i] == 'i' || 
            kernel[i] == 'o' || kernel[i] == 'u' || kernel[i] == 'y') {
            kernel[i] = letter;
            count++;
        }
    }
    printk(KERN_INFO "after: %s\n", kernel);

    if (copy_to_user(user_msg, kernel, len))
       return -EFAULT;
        
    return count;
}

