ifeq ($(KLIBC), 1)
CFLAGS_KLIBC ?= -shared -I /usr/include
CC := klcc
endif

CFLAGS ?= -std=c99 -Wall -pedantic $(CFLAGS_KLIBC)

all: raw-alsa-player

clean:
	rm -f raw-alsa-player
.PHONY: all clean
