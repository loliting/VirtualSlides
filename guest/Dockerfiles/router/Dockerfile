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

RUN systemctl enable iptables-legacy-restore
RUN systemctl enable ifupdown-wait-online.service
RUN systemctl enable kea-dhcp4-server

RUN systemctl mask serial-getty@hvc0.service
RUN systemctl mask serial-getty@hvc1.service

RUN systemctl set-default multi-user.target

COPY vs_init /sbin/vs_init
RUN ln -sf /sbin/vs_init /sbin/reboot
RUN ln -sf /sbin/vs_init /sbin/poweroff
RUN ln -sf /sbin/vs_init /sbin/shutdown

RUN if [[ $DEBUG -eq 1 ]] ; then ln -sf /sbin/vs_init /sbin/vs_test ; fi