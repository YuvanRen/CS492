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
	I pledge my honor that I have abided by the Stevens Honor System
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
#include <linux/uaccess.h>	/* copy_*_user */
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/semaphore.h>
#include "scull.h"		/* local definitions */


/*
 * Our parameters which can be set at load time.
 */

static int scull_major =   SCULL_MAJOR;
static int scull_minor =   0;
static int scull_fifo_elemsz = SCULL_FIFO_ELEMSZ_DEFAULT; /* ELEMSZ */
static int scull_fifo_size   = SCULL_FIFO_SIZE_DEFAULT;   /* N      */

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_fifo_size, int, S_IRUGO);
module_param(scull_fifo_elemsz, int, S_IRUGO);

MODULE_AUTHOR("yrengifo");
MODULE_LICENSE("Dual BSD/GPL");

/* Manage message queue*/
struct ScullDev {
	char *messageQ;
    char *start; /* Oldest pointer*/
    char *end; /* Newest pointer*/
 };

static struct ScullDev sculldev;

/* Mutex/Semaphore*/
static DEFINE_MUTEX(mux);
struct semaphore full;  
struct semaphore empty;

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
 * Read and Write
 */
static ssize_t scull_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	/* TODO: implement this function */
	ssize_t result = 0;

	size_t copies;
	size_t message_len;
	
	printk(KERN_INFO "scull read\n");	/* Helps debugging*/


	if (down_interruptible(&full)) {	/* Check if buffer is empty*/
		printk(KERN_ERR "Buffer is empty, nothing read\n");
		result = -ERESTARTSYS;
		goto done;
        
    }

    if (mutex_lock_interruptible(&mux)) {	/* Check if buffer is locked*/
        up(&full);
		printk(KERN_ERR "Buffer is locked\n");
        result = -ERESTARTSYS;
		goto done;
    }


    message_len = *(size_t *)sculldev.start;

	copies = min(message_len, count); /* Checks amount of bytes that should get copied*/

	/* Copy to buffer */
    if (copy_to_user(buf, sculldev.start + sizeof(size_t), copies)) {
		printk(KERN_ERR "Failed copying to user\n");
        result = -EFAULT;
        goto done;
    }

	/* Move start pointer*/
	sculldev.start += sizeof(size_t) + scull_fifo_elemsz;

	/* Wrap back to the start correctly*/
    if (sculldev.start >= (sculldev.messageQ + ((scull_fifo_elemsz + sizeof(size_t)) * (scull_fifo_size - 1)))) {
        sculldev.start = sculldev.messageQ;
    }


	result = copies;

	mutex_unlock(&mux);
    up(&empty); /* empty spots, been read*/

done:
	return result;
}


static ssize_t scull_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	/* TODO: implement this function */ //producer
	
	ssize_t result = -1;
    size_t copies;

	printk(KERN_INFO "scull write\n"); /* Helps debugging */

	/* Interrupts*/
	if (down_interruptible(&empty)) {
		printk(KERN_ERR "Interrupted");
        result = -ERESTARTSYS; 
		goto done;
    }

	/* Interrupted mux ?*/
    if (mutex_lock_interruptible(&mux)) {
        up(&empty);
		printk(KERN_ERR "Interrupted");
        result = -ERESTARTSYS; 
		goto done;
    }

    copies = min(count, (size_t)scull_fifo_elemsz);	/* Check amount of bytes to be copied*/

    *(size_t *)sculldev.end = copies; /* Put length of message at start of buffer (pointed at by end)*/

    /* Copy from buffer */
    if (copy_from_user(sculldev.end + sizeof(size_t), buf, copies)) {
		printk(KERN_ERR "Failed copying from user");
        result = -EFAULT;
        goto done;
    }

    /* Move end pointer */
    sculldev.end += sizeof(size_t) + scull_fifo_elemsz;

    /* Again make sure we wrap correctly */
    if (sculldev.end >= (sculldev.messageQ + ((scull_fifo_elemsz + sizeof(size_t)) * (scull_fifo_size - 1)))){
		sculldev.end = sculldev.messageQ;
	}

    result = copies;

	mutex_unlock(&mux);
    up(&full);

done:
    return result;
}

/*
 * The ioctl() implementation
 */
static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0;
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
	case SCULL_IOCGETELEMSZ:
		return scull_fifo_elemsz;

	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;

}

struct file_operations scull_fops = {
	.owner 		= THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open 		= scull_open,
	.release	= scull_release,
	.read 		= scull_read,
	.write 		= scull_write,
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

	/* TODO: free FIFO safely here */
	kfree(sculldev.messageQ);
    mutex_destroy(&mux);

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

	/* TODO: allocate FIFO correctly here */
    sculldev.messageQ = kmalloc(scull_fifo_size * (sizeof(size_t) + scull_fifo_elemsz), GFP_KERNEL);

	/* Dynamic allocation checker*/
	if (!sculldev.messageQ) {
		printk(KERN_ERR "Failed to allocate memory\n");
		result = - ENOMEM;
		goto fail;
	}
	/* Start pointers*/

	sculldev.start = sculldev.messageQ;
	sculldev.end = sculldev.messageQ;

    /* Start semaphores*/
    sema_init(&empty, scull_fifo_size);
    sema_init(&full, 0);

    /* Start mutex*/
    mutex_init(&mux);

	printk(KERN_INFO "scull: FIFO SIZE=%u, ELEMSZ=%u\n", scull_fifo_size, scull_fifo_elemsz);
	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);



