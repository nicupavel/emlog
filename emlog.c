
/*
 * EMLOG: the EMbedded-device LOGger
 *
 * Jeremy Elson <jelson@circlemud.org>
 * USC/Information Sciences Institute
 * 
 * Modified By:
 * Andreas Neustifter <andreas.neustifter at gmail.com>
 * Andriy Stepanov <stanv at altlinux.ru>
 * Nicu Pavel <npavel at mini-box.com>
 * Darien Kindlund <kindlund at mitre.org>
 *
 * This code is released under the GPL
 *
 * This is emlog version 0.41, released 13 August 2001, modified 25 July 2006
 * For more information see http://www.circlemud.org/~jelson/software/emlog
 *
 * $Id: emlog.c,v 1.7 2001/08/13 21:29:20 jelson Exp $
 */

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#include <linux/autoconf.h>
#else
#include <generated/autoconf.h>
#endif
#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/poll.h>

#include <asm/uaccess.h>
#include <linux/miscdevice.h>

#include "emlog.h"

struct emlog_info *emlog_info_list = NULL;
static int emlog_debug;

static dev_t emlog_dev_type = 0;
#define EMLOG_MINOR_BASE    0
#define EMLOG_MINOR_COUNT   256
static struct cdev *emlog_cdev = 0;
static struct class *emlog_class = 0;
static struct device *emlog_dev_reg;

module_param(emlog_debug, int, 0644);

#define MIN(x, y) ((x) < (y) ? (x) : y)

/* find the emlog-info structure associated with an inode.  returns a
 * pointer to the structure if found, NULL if not found */
static struct emlog_info *get_einfo(struct inode *inode)
{
    struct emlog_info *einfo;

    if (inode == NULL)
        return NULL;

    for (einfo = emlog_info_list; einfo != NULL; einfo = einfo->next)
        if (einfo->i_ino == inode->i_ino && einfo->i_rdev == inode->i_rdev)
            return einfo;

    return NULL;
}

/* create a new emlog buffer and its associated info structure.
 * returns an errno on failure, or 0 on success.  on success, the
 * pointer to the new struct is passed back using peinfo */
static int create_einfo(struct inode *inode, int minor,
                        struct emlog_info **peinfo)
{
    struct emlog_info *einfo;

    /* make sure the memory requirement is legal */
    if (minor < 1 || minor > EMLOG_MAX_SIZE)
        return -EINVAL;

    /* allocate space for our metadata and initialize it */
    if ((einfo = kmalloc(sizeof(struct emlog_info), GFP_KERNEL)) == NULL)
        goto struct_malloc_failed;

    memset(einfo, 0, sizeof(struct emlog_info));
    einfo->i_ino = inode->i_ino;
    einfo->i_rdev = inode->i_rdev;

    init_waitqueue_head(EMLOG_READQ(einfo));

    /* figure out how much of a buffer this should be and allocate the buffer */
    einfo->size = 1024 * minor;
    if ((einfo->data =
         (char *) vmalloc(sizeof(char) * einfo->size)) == NULL)
        goto data_malloc_failed;

    /* add it to our linked list */
    einfo->next = emlog_info_list;
    emlog_info_list = einfo;

    if (emlog_debug)
        printk(KERN_DEBUG "%s: allocating resources associated with inode %ld.\n", DEVICE_NAME, einfo->i_ino);

    /* pass the struct back */
    *peinfo = einfo;
    return 0;

#if 0
  other_failure:               /* if we check for other errors later, jump here */
#endif
    vfree(einfo->data);
  data_malloc_failed:
    kfree(einfo);
  struct_malloc_failed:
    return -ENOMEM;
}

/* this frees all data associated with an emlog_info buffer, including
 * the struct that you pass to the function.  don't dereference this
 * structure after calling free_einfo! */
void free_einfo(struct emlog_info *einfo)
{
    struct emlog_info **ptr;

    if (einfo == NULL) {
        printk(KERN_ERR "%s: null passed to free_einfo.\n", DEVICE_NAME);
        return;
    }

    if (emlog_debug)
        printk(KERN_DEBUG "%s: freeing resources associated with inode %ld.\n", DEVICE_NAME, einfo->i_ino);

    vfree(einfo->data);

    /* now delete the 'einfo' structure from the linked list.  'ptr' is
     * the pointer that needs to be changed... which is either the list
     * head or one of the 'next' pointers on the list. */
    ptr = &emlog_info_list;
    while (*ptr != einfo) {
        if (!*ptr) {
            printk(KERN_ERR "%s: corrupt einfo list.\n", DEVICE_NAME);
            break;
        } else
            ptr = &((**ptr).next);

    }
    *ptr = einfo->next;

}

/************************ File Interface Functions ************************/

static int emlog_open(struct inode *inode, struct file *file)
{
    int minor = MINOR(inode->i_rdev);

    struct emlog_info *einfo = NULL;
    int retval;

    if ((einfo = get_einfo(inode)) == NULL) {
        /* never heard of this inode before... create a new record */
        if ((retval = create_einfo(inode, minor, &einfo)) < 0)
            return retval;
    }

    if (einfo == NULL) {
        printk(KERN_ERR "%s: can not fetch einfo for inode %ld, in emlog_open.\n", DEVICE_NAME, inode->i_ino);
        return -EIO;
    }

    einfo->refcount++;
    try_module_get(THIS_MODULE);
    return 0;
}

/* this is called when a file is closed */
static int emlog_release(struct inode *inode, struct file *file)
{
    struct emlog_info *einfo;
    int retval = 0;

    /* get the buffer info */
    if ((einfo = get_einfo(inode)) == NULL) {
        printk(KERN_ERR "%s: can not fetch einfo for inode %ld, in emlog_release.\n", DEVICE_NAME, inode->i_ino);
        retval = EIO; goto out;
    }

    /* decrement the reference count.  if no one has this file open and
     * it's not holding any data, delete the record. */
    einfo->refcount--;

    if (einfo->refcount == 0 && EMLOG_QLEN(einfo) == 0)
        free_einfo(einfo);

  out:
    module_put(THIS_MODULE);
    return retval;
}

/* read_from_emlog reads bytes out of a circular buffer with
 * wraparound.  returns caddr_t, pointer to data read, which the
 * caller must free.  length is (a pointer to) the number of bytes to
 * be read, which will be set by this function to be the number of
 * bytes actually returned */
caddr_t read_from_emlog(struct emlog_info * einfo, size_t * length,
                        loff_t * offset)
{
    caddr_t retval;
    int bytes_copied = 0, n, start_point, remaining;

    /* is the user trying to read data that has already scrolled off? */
    if (*offset < einfo->offset)
        *offset = einfo->offset;

    /* is the user trying to read past EOF? */
    if (*offset >= EMLOG_FIRST_EMPTY_BYTE(einfo))
        return NULL;

    /* find the smaller of the total bytes we have available and what
     * the user is asking for */
    *length = MIN(*length, EMLOG_FIRST_EMPTY_BYTE(einfo) - *offset);
    remaining = *length;

    /* figure out where to start based on user's offset */
    start_point = einfo->read_point + (*offset - einfo->offset);
    start_point = start_point % einfo->size;

    /* allocate memory to return */
    if ((retval = kmalloc(sizeof(char) * remaining, GFP_KERNEL)) == NULL)
        return NULL;

    /* copy the (possibly noncontiguous) data to our buffer */
    while (remaining) {
        n = MIN(remaining, einfo->size - start_point);
        memcpy(retval + bytes_copied, einfo->data + start_point, n);
        bytes_copied += n;
        remaining -= n;
        start_point = (start_point + n) % einfo->size;
    }

    /* advance user's file pointer */
    *offset += *length;
    return retval;
}

static ssize_t emlog_read(struct file *file, char *buffer,      /* The buffer to fill with data */
                          size_t length,        /* The length of the buffer */
                          loff_t * offset)
{                               /* Our offset in the file */
    int retval;
    caddr_t data_to_return;
    struct emlog_info *einfo;

    /* get the metadata about this emlog */
    if ((einfo = get_einfo(file->f_dentry->d_inode)) == NULL) {
        printk(KERN_ERR "%s: can not fetch einfo for inode %ld, in emlog_release.\n", DEVICE_NAME, (long)(file->f_dentry->d_inode->i_ino));
        return -EIO;
    }

    /* wait until there's data available (unless we do nonblocking reads) */
    if (file->f_flags & O_NONBLOCK
        && *offset >= EMLOG_FIRST_EMPTY_BYTE(einfo))
        return -EAGAIN;

    wait_event_interruptible((einfo)->read_q,
                             *offset <
                             (einfo)->offset + EMLOG_QLEN(einfo));

    /* see if a signal woke us up */
    if (signal_pending(current))
        return -ERESTARTSYS;

    if ((data_to_return = read_from_emlog(einfo, &length, offset)) == NULL)
        return 0;

    if (copy_to_user(buffer, data_to_return, length) > 0)
        retval = -EFAULT;
    else
        retval = length;
    kfree(data_to_return);
    return retval;
}

/* write_to_emlog writes to a circular buffer with wraparound.  in the
 * case of an overflow, it overwrites the oldest unread data. */
void write_to_emlog(struct emlog_info *einfo, caddr_t buf, int length)
{
    int bytes_copied = 0;
    int overflow = 0;
    int n;

    if (length + EMLOG_QLEN(einfo) >= (einfo->size - 1)) {
        overflow = 1;

        /* in case of overflow, figure out where the new buffer will
         * begin.  we start by figuring out where the current buffer ENDS:
         * einfo->offset + EMLOG_QLEN.  we then advance the end-offset
         * by the length of the current write, and work backwards to
         * figure out what the oldest unoverwritten data will be (i.e.,
         * size of the buffer).  was that all quite clear? :-) */
        einfo->offset = einfo->offset + EMLOG_QLEN(einfo) + length
            - einfo->size + 1;
    }

    while (length) {
        /* how many contiguous bytes are available from the write point to
         * the end of the circular buffer? */
        n = MIN(length, einfo->size - einfo->write_point);
        memcpy(einfo->data + einfo->write_point, buf + bytes_copied, n);
        bytes_copied += n;
        length -= n;
        einfo->write_point = (einfo->write_point + n) % einfo->size;
    }

    /* if there is an overflow, reset the read point to read whatever is
     * the oldest data that we have, that has not yet been
     * overwritten. */
    if (overflow)
        einfo->read_point = (einfo->write_point + 1) % einfo->size;
}

static ssize_t emlog_write(struct file *file,
                           const char *buffer,
                           size_t length, loff_t * offset)
{
    caddr_t message = NULL;
    int n;
    struct emlog_info *einfo;

    /* get the metadata about this emlog */
    if ((einfo = get_einfo(file->f_dentry->d_inode)) == NULL)
        return -EIO;

    /* if the message is longer than the buffer, just take the beginning
     * of it, in hopes that the reader (if any) will have time to read
     * before we wrap around and obliterate it */
    n = MIN(length, einfo->size - 1);

    /* make sure we have the memory for it */
    if ((message = kmalloc(n, GFP_KERNEL)) == NULL)
        return -ENOMEM;

    /* copy into our temp buffer */
    if (copy_from_user(message, buffer, n) > 0) {
        kfree(message);
        return -EFAULT;
    }

    /* now copy it into the circular buffer and free our temp copy */
    write_to_emlog(einfo, message, n);
    kfree(message);

    /* wake up any readers that might be waiting for the data.  we call
     * schedule in the vague hope that a reader will run before the
     * writer's next write, to avoid losing data. */
    wake_up_interruptible(EMLOG_READQ(einfo));

    return n;
}

static unsigned int emlog_poll(struct file *file, poll_table * wait)
{
    struct emlog_info *einfo;

    /* get the metadata about this emlog */
    if ((einfo = get_einfo(file->f_dentry->d_inode)) == NULL)
        return -EIO;

    poll_wait(file, EMLOG_READQ(einfo), wait);

    if (file->f_pos < EMLOG_FIRST_EMPTY_BYTE(einfo))
        return POLLIN | POLLRDNORM;
    else
        return 0;
}

static struct file_operations emlog_fops = {
    .read = emlog_read,
    .write = emlog_write,
    .open = emlog_open,
    .release = emlog_release,
    .poll = emlog_poll,
    .llseek = no_llseek,        /* no_llseek by default introduced at v2.6.37-rc1 */
    .owner = THIS_MODULE,
};

static int __init emlog_init(void)
{
    int ret_val;

    ret_val = alloc_chrdev_region(&emlog_dev_type, EMLOG_MINOR_BASE, EMLOG_MINOR_COUNT, DEVICE_NAME);
    if(ret_val < 0) {
        printk(KERN_ERR "%s: Can not alloc_chrdev_region, error code %d.\n", DEVICE_NAME, ret_val);
        return -1;
    }

    emlog_cdev = cdev_alloc();
    if (emlog_cdev == NULL) {
        printk(KERN_ERR "%s: Can not cdev_alloc.\n", DEVICE_NAME);
        ret_val = -2; goto emlog_init_error;
    }

    emlog_cdev->ops = &emlog_fops;
    emlog_cdev->owner = THIS_MODULE;

    ret_val = cdev_add(emlog_cdev, emlog_dev_type, EMLOG_MINOR_COUNT);
    if(ret_val < 0) {
        printk(KERN_ERR "%s: Can not cdev_add, error code %d.\n", DEVICE_NAME, ret_val);
        ret_val = -3; goto emlog_init_error;
    }

    printk(KERN_INFO "%s: version %s running, major is %d, MINOR is %d.\n", DEVICE_NAME, EMLOG_VERSION, MAJOR(emlog_dev_type), MINOR(emlog_dev_type));

    emlog_class = class_create(THIS_MODULE, DEVICE_NAME);
    if (emlog_class == NULL) {
        printk(KERN_ERR "%s: Can not class_create.\n", DEVICE_NAME);
        ret_val = -4; goto emlog_init_error;
    }

    emlog_dev_reg = device_create(emlog_class, NULL, emlog_dev_type, NULL, DEVICE_NAME);
    if (emlog_dev_reg == NULL) {
        printk(KERN_ERR "%s: Can not device_create.\n", DEVICE_NAME);
        ret_val = -5; goto emlog_init_error;
    }

    goto emlog_init_okay;
  emlog_init_error:
    if (emlog_dev_reg) device_destroy(emlog_class, emlog_dev_type);
    if (emlog_class) class_destroy(emlog_class);
    if (emlog_cdev) cdev_del(emlog_cdev);
    if (emlog_dev_type) unregister_chrdev_region(emlog_dev_type, EMLOG_MINOR_COUNT);
  emlog_init_okay:
    return ret_val;
}

static void __exit emlog_remove(void)
{
    /* clean up any still-allocated memory */
    while (emlog_info_list != NULL)
        free_einfo(emlog_info_list);

    device_destroy(emlog_class, emlog_dev_type);
    class_destroy(emlog_class);
    cdev_del(emlog_cdev);
    unregister_chrdev_region(emlog_dev_type, EMLOG_MINOR_COUNT);

    printk(KERN_INFO "%s: unloaded.\n", DEVICE_NAME);
}

module_init(emlog_init);
module_exit(emlog_remove);

MODULE_LICENSE("GPL v2");

