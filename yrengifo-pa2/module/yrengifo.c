#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <asm/current.h>

static int hello_in(void){
  printk(KERN_ALERT "Hello World from YUVAN RENGIFO(yrengifo)");
    return 0;
}

static void PID_ex(void){
  printk(KERN_INFO "PID is %i and program name is %s\n", current->pid,current->comm);
}
module_init(hello_in);
module_exit(PID_ex);
MODULE_LICENSE("Dual BSD/GPL");
