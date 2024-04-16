#!/bin/bash

on_error () {
    if [ ! $? -eq 0 ]; then
        >&2 echo $1 
        popd >> /dev/null
        exit 1
    fi
}


pushd $(dirname $(readlink -f $0)) >> /dev/null

# Linux 6.7.8 is the lastest stable release as of 30.03.2024
LINUX_VER="${VS_LINUX_VER:-"6.8.2"}"
KERNEL_URL=https://cdn.kernel.org/pub/linux/kernel/v$(echo $LINUX_VER | head -c 1).x/linux-$LINUX_VER.tar.xz


if [ ! -d "linux-$LINUX_VER" ]; then
    if [ ! -r "linux-$LINUX_VER.tar.xz" ]; then
        wget $KERNEL_URL
        on_error "Downloading kernel-$LINUX_VER from $KERNEL_URL failed"

    fi
    tar -xf linux-$LINUX_VER.tar.xz
    on_error 'tar failed'

fi

cp defconfig linux-$LINUX_VER/.config

pushd linux-$LINUX_VER >> /dev/null

sed -i 's/#define N_TTY_BUF_SIZE.*/#define N_TTY_BUF_SIZE 1024 * 1024 \/\/ 1MiB/g' ./include/linux/tty.h
on_error "Failed to change kernel's tty buffor size define in source code"

make olddefconfig
on_error 'Kernel config build failed'

make -j$(nproc)
on_error 'Kernel build failed'

cp -f ./arch/x86/boot/bzImage ..

popd >> /dev/null
popd >> /dev/null