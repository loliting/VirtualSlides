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
COPY vs_run /sbin/vs_run
RUN touch /firstboot