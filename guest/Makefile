CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -static
HDRS     = vq_driver.h

all: vq_test

vq_test: vq_test.c vq_driver.c $(HDRS)
	$(CC) $(CFLAGS) -o $@ vq_test.c vq_driver.c

clean:
	rm -f vq_test

.PHONY: all clean
