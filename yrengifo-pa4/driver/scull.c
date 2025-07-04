/*
 * scull.c -- the bare scull char module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

/*
	Yuvan Rengifo
	I pledge my honor that I have abided by the Stevens Honor Sytesm
*/
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/cdev.h>

#include <linux/sched.h>  	/* Access task with current -> str*/
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include <linux/uaccess.h>	/* copy_*_user */

#include "scull.h"		/* local definitions */

/*
 * Our parameters which can be set at load time.
 */

static int scull_major =   SCULL_MAJOR;
static int scull_minor =   0;
static int scull_quantum = SCULL_QUANTUM;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);

MODULE_AUTHOR("yrengifo");
MODULE_LICENSE("Dual BSD/GPL");

static struct cdev scull_cdev;		/* Char device structure */

/*
 * Open and close
 */

static int scull_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull open\n");
	return 0;          /* success */
}

static int scull_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull close\n");
	return 0;
}

/*
 * The ioctl() implementation
 */

/* 4.1 Linked List Struct*/
struct task_node {
    int pid;
    int tgid;
    struct list_head list;
};
static LIST_HEAD(task_list);	
static DEFINE_MUTEX(task_list_mutex);	

static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, tmp;
	int retval = 0;
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {

	case SCULL_IOCRESET:
		scull_quantum = SCULL_QUANTUM;
		break;
        
	case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
		retval = __get_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
		scull_quantum = arg;
		break;

	case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
		retval = __put_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
		return scull_quantum;

	case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */
		tmp = scull_quantum;
		retval = __get_user(scull_quantum, (int __user *)arg);
		if (retval == 0)
			retval = __put_user(tmp, (int __user *)arg);
		break;

	case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */
		tmp = scull_quantum;
		scull_quantum = arg;
		return tmp;

/////////////////////////////////////////////////////////////////////////////////////////////////////////

	case SCULL_IOCIQUANTUM: { /* Info: 3.1 implementation: Print and copy struct task_info */
	struct task_node *node, *temp;	/* 4.1 For linked list*/
	struct task_info kr;			/* 3.1 Create Struct*/
    int found = 0;					/* 4.1 var helper*/

    mutex_lock(&task_list_mutex); /* Lock Mutext*/

	/* 4.1 Matching pid and tgid */
    list_for_each_entry(temp, &task_list, list) { 
        if (temp->pid == current->pid && temp->tgid == current->tgid) {
            found = 1;
            break;
        }
    }
    if (!found) { /* If not matched/found then create new node*/ 

        node = kmalloc(sizeof(*node), GFP_KERNEL); /* Node dynamic allocation*/

        if (!node) { /* Error handler */
			printk(KERN_ERR "kmalloc failed\n");
            return -ENOMEM;

        } else {
			/* 4.1 Asessing info to node*/
            node->pid = current->pid;
            node->tgid = current->tgid;
            list_add_tail(&node->list, &task_list); /* Add to end of list*/
        }
    }

    mutex_unlock(&task_list_mutex); /* Unluck Mutex */

	/* 3.2 Get Task info using current macro*/
    kr.state = !task_is_running(current);
    kr.cpu = task_cpu(current);
    kr.prio = current->prio;
    kr.pid = current->pid;
    kr.tgid = current->tgid;
    kr.nvcsw = current->nvcsw;
    kr.nivcsw = current->nivcsw;

	/* Copy to user space*/
    retval = copy_to_user((struct task_info __user *)arg, &kr, sizeof(kr));

	break;
	}


	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;
}

struct file_operations scull_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open =     scull_open,
	.release =  scull_release,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void scull_cleanup_module(void)
{
	dev_t devno = MKDEV(scull_major, scull_minor);
	struct task_node *node, *temp; 
    int taskNumber = 1;

    mutex_lock(&task_list_mutex); /* Lock Mutex */

	/* 4.1 Task printing linked list*/
    list_for_each_entry(temp, &task_list, list) {
        printk(KERN_INFO "Task %d: PID %d, TGID %d\n", taskNumber, temp->pid, temp->tgid); // Print the information from the linked list.
		taskNumber++;
    }

    /* Deallocate nodes*/
    list_for_each_entry_safe(node, temp, &task_list, list) {
        list_del(&node->list);
        kfree(node);
    }
    mutex_unlock(&task_list_mutex); //unlock the mutex

	/* Get rid of the char dev entry */
	cdev_del(&scull_cdev);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);
}

int scull_init_module(void)
{
	int result;
	dev_t dev = 0;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, 1, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, 1, "scull");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	cdev_init(&scull_cdev, &scull_fops);
	scull_cdev.owner = THIS_MODULE;
	result = cdev_add (&scull_cdev, dev, 1);
	/* Fail gracefully if need be */
	if (result) {
		printk(KERN_NOTICE "Error %d adding scull character device", result);
		goto fail;
	}

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
