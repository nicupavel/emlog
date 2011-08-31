KERNEL_HOME := /usr/src/linux
CFLAGS := -O2 -Wall

DEFAULT_TARGETS := emlog.o nbcat

########

default: $(DEFAULT_TARGETS)

emlog.o: emlog.c emlog.h
	gcc $(CFLAGS) -c -I$(KERNEL_HOME)/include -D__KERNEL__ -DMODULE emlog.c

nbcat: nbcat.c
	gcc $(CFLAGS) -o nbcat nbcat.c

clean:
	rm -f $(DEFAULT_TARGETS)
