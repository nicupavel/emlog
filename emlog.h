
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

 * This code is released under the GPL
 *
 * This is emlog version 0.41, released 13 August 2001, modified 21 February 2005
 * For more information see http://www.circlemud.org/~jelson/software/emlog
 *
 * $Id: emlog.h,v 1.6 2001/08/13 21:29:20 jelson Exp $
 */

#define EMLOG_MAX_SIZE       128        /* max size in kilobytes of a buffer */
#define DEVICE_NAME "emlog"

#define EMLOG_VERSION        "0.52"

/************************ Private Definitions *****************************/

struct emlog_info {
    wait_queue_head_t read_q;
    unsigned long i_ino;        /* Inode number of this emlog buffer */
    dev_t i_rdev;               /* Device number of this emlog buffer */
    char *data;                 /* The circular buffer data */
    int size;                   /* Size of the buffer pointed to by 'data' */
    int refcount;               /* Files that have this buffer open */
    int read_point;             /* Offset in circ. buffer of oldest data */
    int write_point;            /* Offset in circ. buffer of newest data */
    int offset;                 /* Byte number of read_point in the stream */
    struct emlog_info *next;
};

/* amount of data in the queue */
#define EMLOG_QLEN(einfo) ( (einfo)->write_point >= (einfo)->read_point ? \
         (einfo)->write_point - (einfo)->read_point : \
         (einfo)->size - (einfo)->read_point + (einfo)->write_point)

/* byte number of the last byte in the queue */
#define EMLOG_FIRST_EMPTY_BYTE(einfo) ((einfo)->offset + EMLOG_QLEN(einfo))

/* macro to make it more convenient to access the wait q */
#define EMLOG_READQ(einfo) (&((einfo)->read_q))
