# By default, the build is done against the running kernel version.
# to build against a different kernel version, set KVER
#
#  make KVER=2.6.11-alpha
#
#  Alternatively, set KDIR
#
#  make KDIR=/usr/src/linux

KVER ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KVER)/build
MDIR := emlog

obj-m += emlog.o

all:: emlog.h

all::
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

install:: all
	$(MAKE) INSTALL_MOD_PATH=$(DESTDIR) INSTALL_MOD_DIR=$(MDIR) \
		-C $(KDIR) M=$(CURDIR) modules_install

clean::
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
