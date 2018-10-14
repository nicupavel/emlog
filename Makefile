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
MVER := 0.70

DKMS ?= dkms

CFLAGS ?= -Wall -O2
BINDIR ?= $(DESTDIR)/usr/bin

.PHONY: modules modules_install modules_clean nbcat_install nbcat_clean mkemlog_install mkemlog_clean dkms_install dkms_remove

all: modules nbcat mkemlog

install: modules_install nbcat_install mkemlog_install

clean: modules_clean nbcat_clean mkemlog_clean

modules:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

modules_install: modules
	$(MAKE) INSTALL_MOD_PATH=$(DESTDIR) INSTALL_MOD_DIR=$(MDIR) \
		-C $(KDIR) M=$(CURDIR) modules_install

modules_clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean

nbcat: nbcat.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

mkemlog: mkemlog.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

nbcat_install: nbcat
	install -m 0755 -d '$(BINDIR)'
	install -m 0755 -s -t '$(BINDIR)' nbcat

mkemlog_install: mkemlog
	install -m 0755 -d '$(BINDIR)'
	install -m 0755 -s -t '$(BINDIR)' mkemlog

nbcat_clean:
	rm -f nbcat

mkemlog_clean:
	rm -f mkemlog

dkms_install:
	$(DKMS) add .
	$(DKMS) install emlog/$(MVER)

dkms_remove:
	$(DKMS) remove emlog/$(MVER) --all
	#rm -rf /usr/src/emlog-$(MVER)
