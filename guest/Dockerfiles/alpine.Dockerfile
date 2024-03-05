FROM alpine:latest

RUN apk update && apk add \
    openrc \
    agetty \
    ifupdown-ng \
    bash \
    file

RUN mkdir -p /etc/network
RUN printf "auto eth0\niface eth0 inet dhcp4\n\n" >> /etc/network/interfaces
RUN printf "auto eth1\niface eth1 inet dhcp4\n\n" >> /etc/network/interfaces

RUN echo 'ttyS0::respawn:/sbin/agetty -L ttyS0 --autologin root 115200 xterm-256color' >> /etc/inittab

COPY init/target/x86_64-unknown-linux-musl/release/guest-init /sbin/vs_init