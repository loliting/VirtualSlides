FROM debian:bookworm

ARG DEBUG

ENV DEBIAN_FRONTEND noninteractive

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

RUN systemctl set-default multi-user.target

COPY vs_init /sbin/vs_init
RUN ln -sf /sbin/vs_init /sbin/reboot
RUN ln -sf /sbin/vs_init /sbin/poweroff
RUN ln -sf /sbin/vs_init /sbin/shutdown

COPY vs_run /sbin/vs_run

RUN if [[ $DEBUG -eq 1 ]] ; then ln -sf /sbin/vs_init /sbin/vs_test ; fi