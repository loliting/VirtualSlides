/*
 * VirtIO userspace vsock support
 *
 * Copyright 2025 Karol Maksymowicz <karol@maksymowicz.xyz>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 */

#ifndef VIRTIO_VSOCK_H
#define VIRTIO_VSOCK_H

#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qemu/fifo8.h"
#include "qom/object.h"
#include "hw/virtio/virtio.h"
#include "hw/qdev-properties.h"
#include "io/channel-socket.h"
#include "io/net-listener.h"
#include "system/iothread.h"

#define TYPE_VIRTIO_VSOCK "virtio-vsock-device"
OBJECT_DECLARE_SIMPLE_TYPE(VirtIOVSock, VIRTIO_VSOCK)
#define VIRTIO_VSOCK_GET_PARENT_CLASS(obj) \
        OBJECT_GET_PARENT_CLASS(obj, TYPE_VIRTIO_VSOCK)

#define VSOCK_USER_CONNECTION_REJECTED 0
#define VSOCK_USER_CONNECTION_ACCEPTED 1

/* Socket is connected on the vm side only */
#define VSOCK_USER_CONNECTION_STATE_CONNECTED_VM (1 << 0)
/* Socket is connected to the host side only */
#define VSOCK_USER_CONNECTION_STATE_CONNECTED_HOST (1 << 1)
/* Socket is connected to both sides */
#define VSOCK_USER_CONNECTION_STATE_CONNECTED \
    ( VSOCK_USER_CONNECTION_STATE_CONNECTED_VM | VSOCK_USER_CONNECTION_STATE_CONNECTED_HOST )
/* Failure or disconnection on either side */
#define VSOCK_USER_CONNECTION_STATE_REJECTED_FAILED_OR_DISCONNECTED (1 << 2)
/* The peer will not receive any more data */
#define VSOCK_USER_CONNECTION_STATE_SHUTDOWN_RCV (1 << 3)
/* The peer will not send any more data */
#define VSOCK_USER_CONNECTION_STATE_SHUTDOWN_SEND (1 << 4)

typedef struct VSockUserConnectionKey {
    uint64_t host_cid;
    uint64_t vm_cid;
    uint32_t host_port;
    uint32_t vm_port;
} QEMU_PACKED VSockUserConnectionKey;

typedef struct VSockUserConnection {
    VirtIOVSock* vsock;
    VSockUserConnectionKey* key;
    QIOChannelSocket* sioc;
    Fifo8 host_buf;
    QemuMutex host_buf_lock;
    uint32_t vm_buf_alloc;
    uint32_t vm_fwd_cnt; // free-running counter
    uint32_t host_fwd_cnt; // free-running counter
    uint32_t host_tx_cnt; // free-running counter
    guint close_handler_watch_source_id;
    uint8_t state;
} QEMU_ALIGNED(8) VSockUserConnection;

typedef struct VirtIOVSockConf {
    uint32_t cid;
    char* guest_uds_path;
    char* host_uds_path;
} VirtIOVSockConf;

struct VirtIOVSock {
    VirtIODevice parent_obj;

    VirtQueue *rx_vq;
    VirtQueue *tx_vq;
    VirtQueue *event_vq;

    GHashTable* connection_hash_table;
    IOThread *io_thread;
    SocketAddress guest_uds_addr;
    SocketAddress host_uds_addr;
    QIONetListener *guest_socket_listener;

    VirtIOVSockConf conf;
};

#endif // VIRTIO_VSOCK_H