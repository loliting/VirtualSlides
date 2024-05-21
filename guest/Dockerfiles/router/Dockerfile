FROM debian:bookworm

ARG DEBUG

RUN apt-get update && apt-get install -y \
    ifupdown \
    kea-dhcp4-server \
    iputils-ping \
    systemd \
    systemd-sysv \
    udev \
    iptables \
    vim

COPY etc /etc

RUN systemctl enable iptables-restore
RUN systemctl enable ifupdown-wait-online.service
RUN systemctl enable kea-dhcp4-server

RUN systemctl set-default multi-user.target

COPY vs_init /sbin/vs_init
COPY vs_run /sbin/vs_run
RUN touch /firstboot