FROM alpine:latest

ARG DEBUG

RUN apk update && apk add \
    openrc \
    agetty \
    ifupdown-ng \
    bash \
    file \
    eudev \
    udev-init-scripts

COPY etc /etc

RUN rc-update add udev sysinit
RUN rc-update add udev-trigger sysinit
RUN rc-update add udev-settle sysinit
RUN rc-update add udev-postmount default

COPY vs_init /sbin/vs_init
RUN ln -sf /sbin/vs_init /sbin/reboot
RUN ln -sf /sbin/vs_init /sbin/poweroff
RUN ln -sf /sbin/vs_init /sbin/shutdown

COPY vs_run /sbin/vs_run

RUN if [[ $DEBUG -eq 1 ]] ; then ln -sf /sbin/vs_init /sbin/vs_test ; fi