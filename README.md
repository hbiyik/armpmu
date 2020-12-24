This kernel module enable user access for Armv6 (ARM1136/56/76), ARMv7 & ARMv8 variants both in aarch32 and aarch64 mode.

The module does not preconfigure pmu control register, instead expects user to configure it from user space.

Module also enables all pmu interrupts when user space access is enabled, and disabled when user space is disabled.

user following syntax to cross compile to your device

```
TOOLCHAIN=prefix-of-your-toolchain ARCH=arch-of-the-kernel:arm/arm64 KDIR=path-to-kernel-src-root TCPATH=path-to-your-toolcahin make
```

you dont have to necesserly use all the env variables, ie: below should work:

```
TOOLCHAIN=arm-linux-gnueabihf ARCH=arm make
```

note: toolchain must not end with -, also make sure you are compiling with a compatible gcc version specifically on aarch64, the ABI of gcc should match.

Check Makefile for details. 