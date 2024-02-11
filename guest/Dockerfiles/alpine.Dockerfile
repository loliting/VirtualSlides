FROM alpine:latest

RUN apk update && apk add \
    openrc \
    agetty \
    bash


COPY init/target/x86_64-unknown-linux-gnu/release/init /sbin/vs_init

RUN echo 'ttyS0::respawn:/sbin/agetty -L ttyS0 --autologin root 115200 xterm-256color' >> /etc/inittab
