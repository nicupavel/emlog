emlog -- the EMbedded-system LOG-device
=======================================

Version 0.52, 04 September 2012

Author:   Jeremy Elson <jelson@circlemud.org>
Web page:
* http://www.circlemud.org/~jelson/software/emlog
* https://github.com/nicupavel/emlog

--------------------------------------------------------------------------


What is emlog?
==============

emlog is a Linux kernel module that makes it easy to access the most
recent (and *only* the most recent) output from a process.  It works
just like "tail -f" on a log file, except that the storage required
never grows.  This can be useful in embedded systems where there isn't
enough memory or disk space for keeping complete log files, but the
most recent debugging messages are sometimes needed (e.g., after an
error is observed).

The emlog kernel module implements simple character device driver.
The driver acts like a named pipe that has a finite, circular buffer.
The size of the buffer is easily configurable.  As more data is
written into the buffer, the oldest data is discarded.  A process that
reads from an emlog device will first read the existing buffer, then
see new text as it's written, similar to monitoring a log file using
"tail -f".  (Non-blocking reads are also supported, if a process needs
to get the current contents of the log without blocking to wait for
new data.)

The current version of emlog should work under just about any Linux
kernel in the 2.6.x and 3.x series.

emlog is free software, distributed under the GNU General Public
License (GPL) version 2; see the file COPYING (in the distribution) for
details.


How is emlog used?
==================

### 1: Configure, compile, and install emlog

   If you want to compile emlog for use with the currently running kernel,
   simply run
   ```bash
   make
   ```

   Otherwise, you have to set either KVER (for linux kernel sources,
   located in `/lib/modules/<KVER>/build`) or KDIR (for any other path):
   ```bash
   make KDIR=/usr/src/linux
   ```

   Two files should be generated: the kernel module itself (`emlog.ko`),
   and the `nbcat` utility that will be described later.
   You can use them directly from the current directory or you can install them via
   ```bash
   make install
   ```


### 2: Load the emlog module into the kernel

   If you chose to use emlog directly from the current directly, insert
   the module into the kernel using the `insmod` command
   ```bash
   insmod emlog.ko
   ```

   Otherwise, `modprobe` should work:
   ```bash
   modprobe emlog
   ```

   If successful, a message similar to
   ```
   emlog:emlog_init: version 0.52 running, major is 251, MINOR is 1.
   ```
   should show up in your kernel log (type `dmesg` to see it).
   You can also verify that the module has been inserted by
   typing `lsmod` or `cat /proc/modules`.


### 3: Create device files for emlog

   By default, a device file `/dev/emlog` is created (if you have devtmpfs
   mounted onto `/dev` and/or have udev running) with a minimal allocated buffer.
   It's ready to be written to/read from.

   If you need more devices/buffers, you should use `mknod` to create device
   files that your processes can write to.
   You need to know two numbers: the major and the minor.
   You can find the major number by either of the following methods:
   ```bash
   ls -l /dev/emlog
   grep emlog /proc/devices
   (source /sys/class/emlog/emlog/uevent ; echo "$MAJOR")
   dmesg | grep emlog
   ```
   The minor number is used to indicate the *size* of the ring
   buffer for that device file, specified as the the number of
   kilobytes (e.g., 1024 bytes).  For example, to create an 8K buffer
   called 'testlog':
   ```bash
   mknod /tmp/testlog c 251 8
   ```

   You can create as many devices as you like.  Internally, emlog uses
   the file's inode and device numbers to identify the buffer to which
   the file refers. Note, that internal buffer size is currently limited to 128K.


### 4: Write to and read from your new device file

   Once the device file has been created, simply write to your device
   file as you would any normal named pipe, e.g.
   ```bash
   echo hello > /tmp/testlog
   ```

   Writes to the log will never block because the buffer never runs
   out of space; old data is simply overwritten by new data.

   You can read from the log in the normal way, e.g. using cat.  By
   default, reads block, just like `tail -f`, waiting for new log
   data.  For example:
   ```bash
   cat /tmp/testlog
   hello  [we immediately see the hello that we wrote in the previous step]
   _      [... and here's the cursor.  the 'cat' process is now
           blocked, waiting for new input.  New data will be displayed
           as it is written to the device by other processes.]
   ^C     [use control-c, for example, to stop reading.]
   ```

   As of version 0.40, emlog's buffers can be read and/or monitored
   by multiple concurrent readers correctly.  Data written to an
   emlog device will not disappear until it is overwritten by newer
   data, or the emlog module is removed.  (In versions 0.30 and
   earlier, data was removed from the buffer the first time it was
   read.)


### 5: Remove emlog when you're done

   Type `rmmod emlog` or `modprobe -r emlog` to remove the emlog kernel
   module and free all associated buffers.  This won't work until all emlog
   device files are closed.


Other Usage Notes
=================

* emlog will allocate a fixed-size buffer on behalf of a device file
if one of the following two conditions is true:

  1)  A process has the file open for reading or writing
  2)  A process has written text to the pipe

In other words, buffers are persistent, even after a process closes
the emlog device.  Therefore, it is possible (naturally) to fill
virtual memory by creating many large emlog devices and writing one
byte to all of them.  Don't do that.  All buffers will be freed when
the emlog kernel module is removed.

* Non-blocking reads work; i.e., setting O_NONBLOCK using ioctl()
will cause an EAGAIN to be returned if there is no data ready.  In
addition, the select() and poll() functions will work correctly on
emlog devices.

* A small utility, `nbcat`, is included with the emlog distribution.
nbcat is similar to `cat`, but uses nonblocking reads.
This utility can be used to copy the current contents of an emlog
device without blocking to wait for more input.  For example:
   ```bash
   nbcat /var/log/emlog-device-instance > /tmp/saved-log-file
   ```
...will copy the current contents of the named emlog device to a file
in `/tmp`.
Alternatively, you can use `dd` for that
   ```bash
   dd if=/var/log/emlog-device-instance of=/tmp/saved-log-file bs=4096 iflag=nonblock 2>/dev/null
   ```


Emlog and devtmpfs
===============

By default, emlog creates only one device in `/dev/emlog` (or whereever
your devtmpfs is mounted) with minimal buffer size.
It doesn't make much sense to precreate devices with all possible buffer sizes.
emlog lets you create as many log devices as you like,
anywhere on the filesystem -- the module tells
them apart based on their inode number.  Having a single log device
always exist in a single place (/dev) is much less useful.


Troubleshooting
===============

Q: I'm seeing "I/O error" at a time *other* then when the module is
inserted.

A:  Oops - you've found a bug in emlog.  Please report it.


Q:  When I try to access an emlog device file for reading or writing,
I get the error "no such device".

A:  This probably means either that the emlog kernel module is not
loaded; or, that the major number of the device file does not match
the major number that emlog registered.  To see which major number is
being used by emlog, use any of the following methods:
```bash
grep emlog /proc/devices
(source /sys/class/emlog/emlog/uevent ; echo "$MAJOR")
dmesg | grep emlog
```


Q:  When I try to access an emlog device file for reading or writing,
I get the error "invalid argument".

A:  The *minor* number of the emlog device file must be a number
between 1 and 128, representing the number of kilobytes (1,024 bytes)
that should be used for emlog's ring buffer.  Make sure you're
specifying a valid minor number in your `mknod` statement.  Don't use
0.


Q:  I see "no memory" errors when I try opening new emlog files.

A:  Looks like you're out of virtual memory, sport.


Q:  When I try to remove the emlog driver (`rmmod emlog`), I get the
error "Device or resource busy" or "rmmod: ERROR: Module emlog is in use".

A:  That means a process is currently using an emlog device.  You have
to wait until all processes close all emlog device files until the
driver can be removed.  Try using `lsof` to see which files are in use
by which processes.


Q:  I am trying to save a copy of the current emlog buffer to another
file, by typing `cp /tmp/emlog-test /tmp/saved-log-copy`, but cp just
sits there forever.

A:  `cp` is blocked waiting for more data, just like `cat` does when
used with an emlog device.  Use `nbcat`, the non-blocking cat utility
included with the emlog distribution; for example:
   ```bash
   nbcat /tmp/emlog-test > /tmp/saved-log-copy
   ```


Q:  You've made my computer crash.

A:  Sorry.  If you can reproduce the problem I'll try to fix it.


Known Bugs
==========

None as of version 0.40.


Bug reports, patches, complaints, praise, and submissions of Central
Services Form 27B/6, are welcomed at [Emlog github page](https://github.com/nicupavel/emlog).


Version History
===============

Version 0.40 (August 13, 2001)
 - Concurrent readers and writers are now supported correctly (data is
   not consumed when it is first read, as it was in previous
   versions).
 - emlog's ring buffers now allocated using vmalloc instead of kmalloc
   to avoid locking large blocks of contiguous physical memory.
 - Added MODVERSIONS support
 - Added the 'nbcat' utility - similar to cat, but does not block at
   the end of the data.
 - Bug fix: both device number and inode number are now stored
   internally (instead of only the inode number).  This prevents the
   (unlikely) possibility that emlogs on different filesystems might
   share a single buffer.

Version 0.30 (March 1, 2001)
 - Now compiles correctly for 2.4 series kernels.
 - select() and poll() now work correctly on emlog devices.
 - Bug fix: all instances should not share one wait queue!

Version 0.20 (June 14, 2000)
 - Initial public release.


Who wrote emlog, and why?
=========================

Emlog was written by Jeremy Elson <jelson@circlemud.org> at the
University of Southern California's Information Sciences Institute as
part of the SCADDS project <http://www.isi.edu/scadds>.  SCADDS is an
embedded systems research project.  We use small PC/104-bus-based
single-board-PCs using Linux.  We wanted to save the debugging output
from certain processes, but since these things have 16MB of disk space
and 32MB of RAM, keeping complete log files was not an option.  These
tiny nodes do have serial ports running PPP, though, so it's possible
to walk over to a node with a laptop, plug in a serial cable, and then
telnet into the box.  Using emlog, we can always keep the most recent
debug messages from our processes; in case of an error, we can plug in
a debug console and see what went wrong.

This work was supported by DARPA under grant No. DABT63-99-1-0011 as
part of the SCADDS project, and was also made possible in part due to
support from Cisco Systems.
