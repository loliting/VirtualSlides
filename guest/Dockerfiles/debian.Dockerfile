FROM debian:bookworm


RUN echo "bookworm" > /etc/hostname

# Configure network
RUN mkdir -p /etc/network
RUN printf "auto eth0\niface eth0 inet dhcp4\n" >> /etc/network/interfaces
RUN printf "auto eth1\niface eth1 inet dhcp4\n" >> /etc/network/interfaces

# Set autologin to root on serial tty, also set TERM to `xterm-256color`
RUN mkdir -p /etc/systemd/system/serial-getty@ttyS0.service.d
RUN echo "[Service]\nExecStart=\nExecStart=-/sbin/agetty -o '-p -f -- \\u' --keep-baud --autologin root 115200,57600,38400,9600 - xterm-256color" >> /etc/systemd/system/serial-getty@ttyS0.service.d/autologin.conf

RUN apt-get update && apt-get install -y \
    systemd \
    systemd-sysv \
    udev \
    ifupdown \
    iputils-ping \
    traceroute \
    htop \
    procps \
    vim \
    nano

RUN systemctl mask serial-getty@hvc0.service
RUN systemctl mask serial-getty@hvc1.service

COPY init/target/x86_64-unknown-linux-musl/release/guest-init /sbin/vs_init