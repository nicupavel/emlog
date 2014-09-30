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

CFLAGS ?= -Wall -O2
BINDIR ?= $(DESTDIR)/usr/bin

all: modules nbcat

install: modules_install nbcat_install

clean: modules_clean nbcat_clean

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

modules_install: modules
	$(MAKE) INSTALL_MOD_PATH=$(DESTDIR) INSTALL_MOD_DIR=$(MDIR) \
		-C $(KDIR) M=$(CURDIR) modules_install

modules_clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

nbcat: nbcat.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

nbcat_install: nbcat
	install -m 0755 -d '$(BINDIR)'
	install -m 0755 -s -t '$(BINDIR)' nbcat

nbcat_clean:
	rm -f nbcat
