CFLAGS ?= -std=c99 -Wall -pedantic

all: raw-alsa-player

clean:
	rm -f raw-alsa-player
.PHONY: all clean
