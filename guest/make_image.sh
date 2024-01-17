#!/bin/bash

DOCKER_BUILDKIT=1


on_error () {
    if [ ! $? -eq 0 ]; then
        >&2 echo $1
        
        # Clean build dir
        rm -f $VS_DIST-large.qcow2
        rm -f $VS_DIST.tar

        popd >> /dev/null
        exit 1
    fi
}


# If we're not in /guest, enter it
pushd $(dirname $(readlink -f $0)) >> /dev/null

# Check if VS_DIST is set
if [ -z $VS_DIST ]; then
    >&2 echo "\$VS_DIST not set."
    popd >> /dev/null
    exit 1    
fi

# If $CC is not set, set it to gcc
CC="${CC:-gcc}"

# Build our init system
if [ -z $VS_INIT_SYS ]; then
    gcc -Wall -o init -static init.c
else
    gcc -Wall -DINIT_SYS="\"$VS_INIT_SYS\"" -o init -static init.c
fi
on_error "Init sys build failed."

# Build .tar fs from specified Dockerfile 
docker build --output "type=tar,dest=$VS_DIST.tar" -f ./Dockerfiles/$VS_DIST.Dockerfile .
on_error "Docker build failed."

# Make .qcow2 image from .tar fs 
virt-make-fs --format=qcow2 --size=+256M $VS_DIST.tar $VS_DIST-large.qcow2
on_error "Creating .qcow2 image failed."

# Trim unused space from the imaage
qemu-img convert $VS_DIST-large.qcow2 -O qcow2 $VS_DIST.qcow2
on_error "Trimming .qcow2 image failed."

# Clean build dir
rm -f $VS_DIST-large.qcow2
rm -f $VS_DIST.tar

popd >> /dev/null