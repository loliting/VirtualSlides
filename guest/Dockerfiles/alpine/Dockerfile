FROM alpine:latest

RUN apk update && apk add \
    openrc \
    agetty \
    ifupdown-ng \
    bash \
    file

COPY etc /etc

COPY vs_init /sbin/vs_init
RUN ln -sf /sbin/vs_init /sbin/reboot
RUN ln -sf /sbin/vs_init /sbin/poweroff
RUN ln -sf /sbin/vs_init /sbin/shutdown