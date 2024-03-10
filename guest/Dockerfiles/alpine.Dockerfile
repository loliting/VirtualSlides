FROM alpine:latest

RUN echo "alpine" > /etc/hostname


RUN apk update && apk add \
    openrc \
    agetty \
    ifupdown-ng \
    bash \
    file

RUN mkdir -p /etc/network
RUN printf "allow-hotplug eth0\niface eth0 inet dhcp\n\n" >> /etc/network/interfaces
RUN printf "allow-hotplug eth1\niface eth1 inet dhcp\n\n" >> /etc/network/interfaces

RUN echo 'ttyS0::respawn:/sbin/agetty -L ttyS0 --autologin root 115200 xterm-256color' >> /etc/inittab

COPY init/target/x86_64-unknown-linux-musl/release/guest-init /sbin/vs_init
RUN ln -sf /sbin/vs_init /sbin/reboot
RUN ln -sf /sbin/vs_init /sbin/poweroff
RUN ln -sf /sbin/vs_init /sbin/shutdown