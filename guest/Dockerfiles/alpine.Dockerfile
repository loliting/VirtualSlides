FROM alpine:latest

RUN apk update && apk add \
    openrc \
    agetty \
    bash \
    file



RUN echo 'ttyS0::respawn:/sbin/agetty -L ttyS0 --autologin root 115200 xterm-256color' >> /etc/inittab

COPY init/target/x86_64-unknown-linux-musl/release/guest-init /sbin/vs_init