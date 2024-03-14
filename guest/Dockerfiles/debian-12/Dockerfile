FROM debian:bookworm

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

COPY etc /etc

RUN systemctl mask serial-getty@hvc0.service
RUN systemctl mask serial-getty@hvc1.service

COPY vs_init /sbin/vs_init
RUN ln -sf /sbin/vs_init /sbin/reboot
RUN ln -sf /sbin/vs_init /sbin/poweroff
RUN ln -sf /sbin/vs_init /sbin/shutdown