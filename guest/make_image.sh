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

pushd ./init >> /dev/null

# Build our init system
CROSS_CONTAINER_OPTS="--platform linux/amd64" cross build --target x86_64-unknown-linux-musl --release
on_error "Init sys build failed."

popd >> /dev/null

# Build .tar fs from specified Dockerfile 
docker build --platform linux/amd64 --output "type=tar,dest=$VS_DIST.tar" -f ./Dockerfiles/$VS_DIST.Dockerfile .
on_error "Docker build failed."

# Make .qcow2 image from .tar fs 
virt-make-fs \
    --format=qcow2 \
    --size=+4G \
    --type=ext4 \
    $VS_DIST.tar $VS_DIST-large.qcow2
on_error "Creating .qcow2 image failed."

# Trim unused space from the image
qemu-img convert $VS_DIST-large.qcow2 -O qcow2 $VS_DIST.qcow2
on_error "Trimming .qcow2 image failed."

# Clean build dir
rm -f $VS_DIST-large.qcow2
rm -f $VS_DIST.tar

popd >> /dev/null