FROM debian:bookworm

COPY init/target/x86_64-unknown-linux-gnu/release/init /sbin/vs_init

RUN echo "Router" > /etc/hostname

# NAT
RUN mkdir -p /etc/iptables
RUN echo "net.ipv4.ip_forward=1" >> /etc/sysctl.conf
RUN echo "*filter\n:INPUT ACCEPT [13:3508]\n:FORWARD ACCEPT [3:252]\n:OUTPUT ACCEPT [3:228]\n-A FORWARD -i eth1 -j ACCEPT\nCOMMIT\n*nat\n:PREROUTING ACCEPT [8:2380]\n:INPUT ACCEPT [7:2296]\n:OUTPUT ACCEPT [3:228]\n:POSTROUTING ACCEPT [0:0]\n-A POSTROUTING -o eth0 -j MASQUERADE\nCOMMIT" > /etc/iptables/rules.v4
RUN echo "[Unit]\nDescription=Load iptables\n\n[Service]\nUser=root\nExecStart=sh -c 'cat /etc/iptables/rules.v4 | iptables-legacy-restore'\n\n[Install]\nWantedBy=multi-user.target" > /etc/systemd/system/iptables-legacy-restore.service

# Set autologin to root on serial tty, also set $TERM to `xterm-256color`
RUN mkdir -p /etc/systemd/system/serial-getty@ttyS0.service.d
RUN echo "[Service]\nExecStart=\nExecStart=-/sbin/agetty -o '-p -f -- \\u' --keep-baud --autologin root 115200,57600,38400,9600 - xterm-256color" > /etc/systemd/system/serial-getty@ttyS0.service.d/autologin.conf


# Configure network interfaces
RUN mkdir -p /etc/network
RUN echo "auto eth0\niface eth0 inet dhcp\n\nauto eth1\niface eth1 inet static\naddress 10.0.64.1\nnetmask 255.255.255.0" > /etc/network/interfaces

RUN apt-get update && apt-get install -y \
    ifupdown \
    kea-dhcp4-server \
    iputils-ping \
    systemd \
    systemd-sysv \
    udev \
    iptables

# Configure DHCPv4 server
RUN mkdir -p /etc/kea
RUN echo "{\"Dhcp4\":{\"interfaces-config\":{\"interfaces\":[\"eth1\"]},\"lease-database\":{\"type\":\"memfile\",\"lfc-interval\":3600},\"valid-lifetime\":600,\"max-valid-lifetime\":7200,\"subnet4\":[{\"id\":1,\"subnet\":\"10.0.64.0/24\",\"pools\":[{\"pool\":\"10.0.64.2 - 10.0.64.240\"}],\"option-data\":[{\"name\":\"routers\",\"data\":\"10.0.64.1\"},{\"name\":\"domain-name-servers\",\"data\":\"1.1.1.1,1.0.0.1\"}]}]}}" > /etc/kea/kea-dhcp4.conf


RUN systemctl enable iptables-legacy-restore
RUN systemctl enable ifupdown-wait-online.service
RUN systemctl enable kea-dhcp4-server
