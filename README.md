# raw-alsa-player

[![Build Status](https://travis-ci.org/blueponies666/raw-alsa-player.svg?branch=master)](https://travis-ci.org/blueponies666/raw-alsa-player)

ALSA audio player using the raw kernel API \(ioctl)

This is a small audio player that is NOT linked to the ALSA library. Instead, it uses the raw kernel interface.

Even though the userspace API claims to have "100% functionality of the kernel API," it does lack some features that are necessary in minimal environments, like:

* The ability to use device nodes located outside of /dev/snd \(useful in chroot)
* Direct access to ioctl calls not implemented in the userspace library
* Static linking and linking to minimal C libraries \(uClibc, klibc, etc.)

libasound is a compile-time dependency, but not a run-time dependency.
