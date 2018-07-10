/*
 * mkemlog: creates emlog files
 *
 * This utility can be used to create emlog files with out having
 * to lookup the major number of /dev/emlog
 *
 * This code is freeware and may be distributed without restriction.
 *
 * James Rouzier <rouzier at gmail.com>
 * July 7, 2016
 *
 * Modified By:
 * David White <udingmyride at gmail.com
 * 25 June 2018
 *
 */

#include <stdio.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>

#define EMLOG_DEVICE "/dev/emlog"

#define USAGE "usage: mkemlog <logdevname> [size_in_kilobytes] [mode] [uid]"

int main(int argc, char** argv) {
    int rc;
    mode_t mode = 0660;
    struct stat emlog_stat;
    int size_of_buffer = 8;
    char* file;
    char* number;
    char* end_of_number;
    int emlog_max_size = 0;
    FILE *max_size_file = NULL;
    uid_t uid = -1;
    if (argc < 2 || argc > 5) {
        error(1 ,0, USAGE);
    }
    file = argv[1];

    max_size_file = fopen("/sys/module/emlog/parameters/emlog_max_size", "r");
    if (max_size_file == NULL)
        error(1, errno, "Emlog module not loaded\n");
    rc = fscanf(max_size_file, "%d", &emlog_max_size);
    if (rc != 1)
        error(1, errno, "Unable to get emlog max size\n");
    fclose(max_size_file);
    max_size_file = NULL;
    if (argc > 2 ) {
        errno = 0;
        number = argv[2];
        size_of_buffer = strtol(number, &end_of_number, 10);
        if (errno) {
            error(1, errno, "Invalid size provided\n" USAGE);
        }
        if (end_of_number == number) {
            error(1, 0, "Invalid size provided\n" USAGE);
        }
        if (size_of_buffer < 1 || size_of_buffer > emlog_max_size) {
            error(1, 0, "Invalid size provided must be a value between 1 and %d\n" USAGE, emlog_max_size);
        }
    }
    if (argc > 3 ) {
        errno = 0;
        number = argv[3];
        mode = strtol(number, &end_of_number, 8);
        if (errno) {
            error(1, errno, "Invalid mode provided\n" USAGE);
        }
        if (end_of_number == number || S_IFMT & mode) {
            error(1, 0, "Invalid mode provided\n" USAGE);
        }
    }
    if (argc > 4 ) {
        errno = 0;
        number = argv[4];
        uid = strtol(number, &end_of_number, 10);
        if (errno) {
            error(1, errno, "Invalid uid provided\n" USAGE);
        }
        if (end_of_number == number) {
            error(1, 0, "Invalid uid provided\n" USAGE);
        }
    }
    rc = stat(EMLOG_DEVICE, &emlog_stat);
    if (rc == -1) {
        error(1, errno, "stat: " EMLOG_DEVICE);
    }
    if (!S_ISCHR(emlog_stat.st_mode)) {
        error(1, 0, EMLOG_DEVICE " is not a valid emlog device\n");
    }
    rc = mknod(file, mode | S_IFCHR, makedev(major(emlog_stat.st_rdev),size_of_buffer));
    if (rc == -1) {
        error(1, errno, "mknod: %s", file);
    }
    if (uid != -1) {
        rc = chown(file, uid, -1);
        if (rc == -1) {
            error(1, errno, "chown: %s", file);
        }
    }
    printf("Log device %s created with buffer size of %d KiB\n", file, size_of_buffer);
    return 0;
}


