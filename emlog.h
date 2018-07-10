
/*
 * EMLOG: the EMbedded-device LOGger
 *
 * Jeremy Elson <jelson@circlemud.org>
 * USC/Information Sciences Institute
 *
 * Modified By:
 * Andreas Neustifter <andreas.neustifter at gmail.com>
 * Andrey Mazo <ahippo at yandex.com>
 * Andriy Stepanov <stanv at altlinux.ru>
 * Darien Kindlund <kindlund at mitre.org>
 * David White <udingmyride at gmail.com>
 * Nicu Pavel <npavel at mini-box.com>

 * This code is released under the GPL
 *
 * This is emlog version 0.70, released 13 August 2001, modified 25 June 2018
 * For more information see http://www.circlemud.org/~jelson/software/emlog
 * and https://github.com/nicupavel/emlog
 *
 * $Id: emlog.h,v 1.6 2001/08/13 21:29:20 jelson Exp $
 */

#define DEVICE_NAME "emlog"

#define EMLOG_VERSION        "0.70"

/************************ Private Definitions *****************************/

struct emlog_info {
    wait_queue_head_t read_q;
    unsigned long i_ino;        /* Inode number of this emlog buffer */
    dev_t i_rdev;               /* Device number of this emlog buffer */
    char *data;                 /* The circular buffer data */
    size_t size;                /* Size of the buffer pointed to by 'data' */
    int refcount;               /* Files that have this buffer open */
    size_t read_point;          /* Offset in circ. buffer of oldest data */
    size_t write_point;         /* Offset in circ. buffer of newest data */
    loff_t offset;              /* Byte number of read_point in the stream */
    struct emlog_info *next;
    rwlock_t rwlock;            /* Per e-info rwlock. This is to protect against concurrent readers/writers */
};

/* amount of data in the queue */
#define EMLOG_QLEN(einfo) ( (einfo)->write_point >= (einfo)->read_point ? \
         (einfo)->write_point - (einfo)->read_point : \
         (einfo)->size - (einfo)->read_point + (einfo)->write_point)

/* byte number of the last byte in the queue */
#define EMLOG_FIRST_EMPTY_BYTE(einfo) ((einfo)->offset + EMLOG_QLEN(einfo))

/* macro to make it more convenient to access the wait q */
#define EMLOG_READQ(einfo) (&((einfo)->read_q))
