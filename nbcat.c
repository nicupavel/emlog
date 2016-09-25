
/*
 * nbcat: a simple program similar to 'cat', but which uses
 * nonblocking reads.
 *
 * This utility can be used to copy the current contents of an emlog
 * device without blocking to wait for more input.  For example:
 *
 *        nbcat /var/log/emlog-device-instance > /tmp/saved-log-file
 *
 * ...will copy the current contents of the named emlog device to a
 * file in /tmp.
 *
 * This code is freeware and may be distributed without restriction.
 *
 * Jeremy Elson <jelson@circlemud.org>
 * August 11, 2001
 *
 * $Id: nbcat.c,v 1.2 2001/08/13 21:29:56 jelson Exp $
 *
 * Modified By:
 * Andreas Neustifter <andreas.neustifter at gmail.com>
 * Nicu Pavel <npavel at mini-box.com>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    int fd, retval;
    char buf[4096];

    if (argc != 2) {
        fprintf(stderr, "usage: %s <filename>\n", argv[0]);
        exit(1);
    }

    if ((fd = open(argv[1], O_RDONLY | O_NONBLOCK)) < 0) {
        perror(argv[1]);
        exit(1);
    }

    while ((retval = read(fd, buf, sizeof(buf))) > 0) {
        if (write(STDOUT_FILENO, buf, retval) < 0) {
            perror("writing to stdout");
            break;
        }
    }

    if (retval < 0 && errno != EAGAIN) {
        perror(argv[1]);
        exit(1);
    }

    return 0;
}
