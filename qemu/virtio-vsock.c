/*
 * VirtIO userspace vsock support
 *
 * Copyright 2025 Karol Maksymowicz <karol@maksymowicz.xyz>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/iov.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/virtio/virtio-vsock.h"
#include "standard-headers/linux/virtio_ids.h"
#include "standard-headers/linux/virtio_vsock.h"
#include "qemu/xxhash.h"
#include "block/aio-wait.h"

#include <sys/ioctl.h>

#define BUF_ALLOC (256 * 1024) // 256 KiB
#define VSOCK_HDR_LEN (sizeof(struct virtio_vsock_hdr))


static gboolean cht_key_cmp(gconstpointer opaque1, gconstpointer opaque2)
{
    const VSockUserConnectionKey* key1 = opaque1;
    const VSockUserConnectionKey* key2 = opaque2;

    return key1->host_cid == key2->host_cid && key1->vm_cid == key2->vm_cid
        && key1->host_port == key2->host_port && key1->vm_port == key2->vm_port;
}

static guint32 cht_key_hash(gconstpointer opaque)
{
    const VSockUserConnectionKey* key = opaque;
    return qemu_xxhash6(key->host_cid, key->vm_cid, key->host_port, key->vm_port);
}

static void cht_destroy_conn(gpointer data) {
    VSockUserConnection* conn = data;
    VirtIOVSock* vsock = conn->vsock;

    if(conn->key) {
        free(conn->key);
        conn->key = NULL;
    }
    fifo8_destroy(&conn->host_buf);
    if(conn->close_handler_watch_source_id) {
        g_source_remove(conn->close_handler_watch_source_id);
        conn->close_handler_watch_source_id = 0;
    }
    if(conn->sioc) {
        AioContext* aio_ctx = iothread_get_aio_context(vsock->io_thread);
        qio_channel_set_aio_fd_handler(QIO_CHANNEL(conn->sioc), aio_ctx,
            NULL, aio_ctx, NULL, NULL); // remove io handlers
        object_unref(OBJECT(conn->sioc));
        conn->sioc = NULL;
    }
    qemu_mutex_destroy(&conn->host_buf_lock);
    free(conn);
}

static inline void virtio_vsock_init_rsp_hdr(struct virtio_vsock_hdr *hdr,
                                             VSockUserConnection* conn)
{
    hdr->src_cid = cpu_to_le64(conn->key->host_cid);
    hdr->src_port = cpu_to_le32(conn->key->host_port);
    hdr->dst_cid = cpu_to_le64(conn->key->vm_cid);
    hdr->dst_port = cpu_to_le32(conn->key->vm_port);
    hdr->type = cpu_to_le16(VIRTIO_VSOCK_TYPE_STREAM);
    hdr->op = cpu_to_le16(VIRTIO_VSOCK_OP_INVALID);
    hdr->buf_alloc = cpu_to_le32(BUF_ALLOC);
    hdr->fwd_cnt = cpu_to_le32(conn->host_fwd_cnt);
    hdr->len = cpu_to_le32(0);
    hdr->flags = cpu_to_le32(0);
}

static void virtio_vsock_init_conn_key(VSockUserConnectionKey *key,
                                       struct virtio_vsock_hdr *hdr)
{
    key->host_cid = le64_to_cpu(hdr->dst_cid);
    key->vm_cid = le64_to_cpu(hdr->src_cid);
    key->host_port = le32_to_cpu(hdr->dst_port);
    key->vm_port = le32_to_cpu(hdr->src_port);
}

static void virtio_vsock_conn_rst(VSockUserConnection* conn)
{
    VirtIOVSock* vsock = conn->vsock;
    VirtQueueElement* elem = NULL;
    struct virtio_vsock_hdr hdr;

    if(!(conn->state & VSOCK_USER_CONNECTION_STATE_CONNECTED_VM)) {
        return;
    }

    elem = virtqueue_pop(conn->vsock->rx_vq, sizeof(VirtQueueElement));
    if(!elem) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueue contains no elements");
        return;
    }
    if(elem->in_num < 1) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueueElement contains no in buffers");
        virtqueue_detach_element(vsock->rx_vq, elem, 0);
        g_free(elem);
        return;
    }

    virtio_vsock_init_rsp_hdr(&hdr, conn);
    hdr.op = VIRTIO_VSOCK_OP_RST;
    
    if(iov_from_buf(elem->in_sg, elem->in_num, 0, &hdr, VSOCK_HDR_LEN) != VSOCK_HDR_LEN) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueueElement's in buffer is too small!");
        virtqueue_detach_element(vsock->rx_vq, elem, 0);
        g_free(elem);
        return;
    }
    virtqueue_push(vsock->rx_vq, elem, sizeof(hdr));
    virtio_notify(VIRTIO_DEVICE(vsock), vsock->rx_vq);
    conn->state = VSOCK_USER_CONNECTION_STATE_REJECTED_FAILED_OR_DISCONNECTED;
    if(conn->sioc) {
        AioContext* aio_ctx = iothread_get_aio_context(vsock->io_thread);
        qio_channel_set_aio_fd_handler(QIO_CHANNEL(conn->sioc), aio_ctx,
            NULL, aio_ctx, NULL, NULL); // remove io handlers
    }
    g_free(elem);
    g_hash_table_remove(conn->vsock->connection_hash_table, conn->key);
}

static void virtio_vsock_conn_credit_update(VSockUserConnection* conn)
{
    VirtIOVSock* vsock = conn->vsock;
    VirtQueueElement* elem = NULL;
    struct virtio_vsock_hdr hdr;

    elem = virtqueue_pop(conn->vsock->rx_vq, sizeof(VirtQueueElement));
    if(!elem) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueue contains no elements");
        return;
    }
    if(elem->in_num < 1) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueueElement contains no in buffers");
        virtqueue_detach_element(vsock->rx_vq, elem, 0);
        g_free(elem);
        return;
    }

    virtio_vsock_init_rsp_hdr(&hdr, conn);
    hdr.op = VIRTIO_VSOCK_OP_CREDIT_UPDATE;
    
    if(iov_from_buf(elem->in_sg, elem->in_num, 0, &hdr, VSOCK_HDR_LEN) != VSOCK_HDR_LEN) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueueElement's in buffer is too small!");
        virtqueue_detach_element(vsock->rx_vq, elem, 0);
        g_free(elem);
        return;
    }
    virtqueue_push(vsock->rx_vq, elem, sizeof(hdr));
    virtio_notify(VIRTIO_DEVICE(vsock), vsock->rx_vq);
}

static gboolean virtio_vsock_sock_closed(QIOChannel *ioc,
                                         GIOCondition condition, gpointer data)
{
    VSockUserConnection* conn = data;
    VirtIOVSock* vsock = conn->vsock;

    if(conn->state & VSOCK_USER_CONNECTION_STATE_CONNECTED_VM) {
        virtio_vsock_conn_rst(conn);
    }
    if(g_hash_table_lookup(vsock->connection_hash_table, conn->key) == NULL) {
        cht_destroy_conn(conn);
    }
    return false;
}

static void virtio_vsock_sock_recv_connected(VSockUserConnection* conn)
{
    VirtIOVSock* vsock = conn->vsock;
    QEMU_UNINITIALIZED VirtQueueElement *elems[VIRTQUEUE_MAX_SIZE];
    QEMU_UNINITIALIZED size_t lens[VIRTQUEUE_MAX_SIZE];
    uint32_t vm_free = 0;
    ssize_t bytes_read = 0;
    size_t elems_len = 0;
    size_t data_sent = 0;
    Error* err = NULL;

    g_assert(conn->state & VSOCK_USER_CONNECTION_STATE_CONNECTED);

    vm_free = conn->vm_buf_alloc - (conn->host_tx_cnt - conn->vm_fwd_cnt);
    if(vm_free == 0) {
        return;
    }
    uint8_t buf[BUF_ALLOC];
    bytes_read = qio_channel_read(QIO_CHANNEL(conn->sioc),
        buf, MIN(BUF_ALLOC, vm_free), &err);
    if(bytes_read == QIO_CHANNEL_ERR_BLOCK) {
        return;
    }
    else if(bytes_read == 0) {
        virtio_vsock_conn_rst(conn);
        return;
    }
    else if(bytes_read < 0) {
        error_report("virtio-vsock: "
            "an error occurred on a socket during read(): %s. "
            "Connection will be closed.", error_get_pretty(err));
        error_free(err);
        virtio_vsock_conn_rst(conn);
        return;
    }

    for(elems_len = 0; data_sent < bytes_read; ++elems_len) {
        VirtQueueElement* elem = NULL;

        elem = virtqueue_pop(vsock->rx_vq, sizeof(VirtQueueElement));
        if(!elem) {
            virtio_error(VIRTIO_DEVICE(vsock),
                "virtio-vsock: rx VirtQueue contains no elements");
            goto err;
        }
        if(elem->in_num < 1) {
            virtio_error(VIRTIO_DEVICE(vsock),
                "virtio-vsock: rx VirtQueueElement contains no in buffers");
            goto err;
        }
        if(elems_len == VIRTQUEUE_MAX_SIZE) {
            virtio_error(VIRTIO_DEVICE(vsock),
                "virtio-vsock: rx VirtQueue is too short");
            goto err;
        }
        lens[elems_len] = iov_from_buf(elem->in_sg, elem->in_num, 
            VSOCK_HDR_LEN, buf + data_sent, bytes_read - data_sent);
        if(lens[elems_len] < 1) {
            virtio_error(VIRTIO_DEVICE(vsock),
                "virtio-vsock: rx VirtQueue in buffer is too small");
            goto err;
        }
        conn->host_tx_cnt += lens[elems_len];
        data_sent += lens[elems_len];
        
        struct virtio_vsock_hdr hdr;
        virtio_vsock_init_rsp_hdr(&hdr, conn);
        hdr.op = cpu_to_le16(VIRTIO_VSOCK_OP_RW);
        hdr.len = cpu_to_le32(lens[elems_len]);
        lens[elems_len] += iov_from_buf(elem->in_sg, elem->in_num, 0, &hdr, VSOCK_HDR_LEN);

        elems[elems_len] = elem;
    }
    g_assert(bytes_read == data_sent);
    
    for (int i = 0; i < elems_len; ++i) {
        virtqueue_fill(vsock->rx_vq, elems[i], lens[i], i);
        g_free(elems[i]);
    }

    virtqueue_flush(vsock->rx_vq, elems_len);
    virtio_notify(VIRTIO_DEVICE(vsock), vsock->rx_vq);
    return;

    err:
    for (int i = 0; i < elems_len; ++i) {
        virtqueue_detach_element(vsock->rx_vq, elems[i], lens[i]);
        g_free(elems[i]);
    }
}

static void virtio_vsock_host_recv_connecting(VSockUserConnection* conn)
{
    VirtIOVSock* vsock = conn->vsock;
    VirtQueueElement* elem = NULL;
    struct virtio_vsock_hdr hdr;
    ssize_t bytes_read = 0;
    uint8_t connection_response = 0;
    Error* err = NULL;
    
    bytes_read = qio_channel_read(QIO_CHANNEL(conn->sioc), &connection_response,
        sizeof(uint8_t), &err);
    if(bytes_read == QIO_CHANNEL_ERR_BLOCK || bytes_read == 0) {
        return;
    }
    else if(bytes_read < 0) {
        error_report("virtio-vsock: "
            "an error occurred on a socket during read(): %s. "
            "Connection will be closed.", error_get_pretty(err));
        error_free(err);
        virtio_vsock_conn_rst(conn);
        return;
    }

    if(connection_response != VSOCK_USER_CONNECTION_ACCEPTED) {
        virtio_vsock_conn_rst(conn);
        return;
    }

    conn->state = VSOCK_USER_CONNECTION_STATE_CONNECTED;
    virtio_vsock_init_rsp_hdr(&hdr, conn);
    hdr.op = cpu_to_le16(VIRTIO_VSOCK_OP_RESPONSE);

    elem = virtqueue_pop(vsock->rx_vq, sizeof(VirtQueueElement));
    if(!elem) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueue contains no elements");
        return;
    }
    if(elem->in_num < 1) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueueElement contains no in buffers");
        virtqueue_detach_element(vsock->rx_vq, elem, 0);
        g_free(elem);
        return;
    }
    if(iov_from_buf(elem->in_sg, elem->in_num, 0, &hdr, VSOCK_HDR_LEN) != VSOCK_HDR_LEN) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueueElement's in buffer is too small!");
        virtqueue_detach_element(vsock->rx_vq, elem, 0);
        g_free(elem);
        return;
    }
    virtqueue_push(vsock->rx_vq, elem, VSOCK_HDR_LEN);
    virtio_notify(VIRTIO_DEVICE(vsock), vsock->rx_vq);
    g_free(elem);
}

static void virtio_vsock_guest_receive_connecting(VSockUserConnection* conn)
{
    VirtIOVSock* vsock = conn->vsock;
    VirtQueueElement* elem = NULL;
    Error* err = NULL;
    ssize_t bytes_read = 0;
    VSockUserConnectionKey connection_response;
    struct virtio_vsock_hdr hdr;
    int pending_bytes = 0;
    
    ioctl(conn->sioc->fd, FIONREAD, &pending_bytes);
    if(pending_bytes < sizeof(VSockUserConnectionKey)) {
        return;
    }

    bytes_read = qio_channel_read(QIO_CHANNEL(conn->sioc), &connection_response,
        sizeof(VSockUserConnectionKey), &err);
    if(bytes_read == QIO_CHANNEL_ERR_BLOCK || bytes_read == 0) {
        return;
    }
    else if(bytes_read != sizeof(VSockUserConnectionKey)) {
        error_report("virtio-vsock: "
            "an error occurred on a socket during read(): %s. "
            "The connection will be closed.", error_get_pretty(err));
        error_free(err);
        cht_destroy_conn(conn);
        return;
    }

    conn->key = g_memdup2(&connection_response, sizeof(VSockUserConnectionKey));
    virtio_vsock_init_rsp_hdr(&hdr, conn);
    hdr.op = cpu_to_le16(VIRTIO_VSOCK_OP_REQUEST);

    elem = virtqueue_pop(vsock->rx_vq, sizeof(VirtQueueElement));
    if(!elem) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueue contains no elements");
        cht_destroy_conn(conn);
        return;
    }
    if(elem->in_num < 1) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueueElement contains no in buffers");
        virtqueue_detach_element(vsock->rx_vq, elem, 0);
        g_free(elem);
        cht_destroy_conn(conn);
        return;
    }
    if(iov_from_buf(elem->in_sg, elem->in_num, 0, &hdr, VSOCK_HDR_LEN) != VSOCK_HDR_LEN) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: rx VirtQueueElement's in buffer is too small!");
        virtqueue_detach_element(vsock->rx_vq, elem, 0);
        g_free(elem);
        cht_destroy_conn(conn);
        return;
    }
    virtqueue_push(vsock->rx_vq, elem, VSOCK_HDR_LEN);
    virtio_notify(VIRTIO_DEVICE(vsock), vsock->rx_vq);
    g_free(elem);

    g_hash_table_insert(vsock->connection_hash_table, conn->key, conn);
}

static void virtio_vsock_do_sock_read_aval(gpointer opaque)
{
    VSockUserConnection* conn = opaque;

    switch (conn->state & ~(VSOCK_USER_CONNECTION_STATE_SHUTDOWN_SEND)) {
    case VSOCK_USER_CONNECTION_STATE_CONNECTED:
    {
        virtio_vsock_sock_recv_connected(conn);
    }
    break;
    case VSOCK_USER_CONNECTION_STATE_CONNECTED_VM:
    {
        virtio_vsock_host_recv_connecting(conn);
    }
    break;
    case VSOCK_USER_CONNECTION_STATE_CONNECTED_HOST:
    {
        if(conn->key == NULL) {
            virtio_vsock_guest_receive_connecting(conn);
        }
    }
    break;
    case VSOCK_USER_CONNECTION_STATE_REJECTED_FAILED_OR_DISCONNECTED:
    {
        g_hash_table_remove(conn->vsock->connection_hash_table, conn->key);
    }
    break;
    }
}

static void virtio_vsock_sock_read_avaliable(gpointer opaque)
{
    aio_bh_schedule_oneshot(qemu_get_aio_context(), 
        virtio_vsock_do_sock_read_aval, opaque);
}

static void virtio_vsock_sock_write_avaliable(gpointer opaque)
{
    VSockUserConnection* conn = opaque;
    VirtIOVSock* vsock = conn->vsock;
    size_t bytes_written = 0;
    const uint8_t* buf = NULL;
    uint32_t buf_sz = 0;
    bool was_fifo_full = fifo8_is_full(&conn->host_buf);
    Error* err = NULL;

    qemu_mutex_lock(&conn->host_buf_lock);

    g_assert((buf_sz = fifo8_num_used(&conn->host_buf)) > 0);
    buf = fifo8_peek_bufptr(&conn->host_buf, buf_sz, &buf_sz);
    bytes_written = qio_channel_write(QIO_CHANNEL(conn->sioc), buf, buf_sz, &err);
    if(bytes_written > 0) {
        fifo8_drop(&conn->host_buf, bytes_written);
        conn->host_fwd_cnt += bytes_written;
    }
    if(fifo8_is_empty(&conn->host_buf)) { // remove write handler
        AioContext* aio_ctx = iothread_get_aio_context(vsock->io_thread);
        qio_channel_set_aio_fd_handler(QIO_CHANNEL(conn->sioc), aio_ctx,
            virtio_vsock_sock_read_avaliable, aio_ctx, NULL, conn); 
    }

    if(was_fifo_full) {
        aio_bh_schedule_oneshot(qemu_get_aio_context(),
            (void (*)(void *))virtio_vsock_conn_credit_update, opaque);
    }
    qemu_mutex_unlock(&conn->host_buf_lock);
}

static void virtio_vsock_guest_new_connection(QIONetListener *listener,
                                                   QIOChannelSocket *ioc,
                                                   gpointer opaque)
{
    // TODO: add timeout
    VirtIOVSock* vsock = opaque;
    VSockUserConnection* conn = NULL;
    IOThread* io_thread = vsock->io_thread;
    AioContext* aio_ctx = iothread_get_aio_context(io_thread);
    Error* err = NULL;

    if(ioc->fd == -1) {
        return;
    }

    if(!qio_channel_set_blocking(QIO_CHANNEL(ioc), false, &err)) {
        error_report("virtio-vsock: "
            "Failed to set new guest-sock connection to non-blocking mode. %s",
            error_get_pretty(err));
        g_free(err); 
        return;
    }
    
    conn = g_malloc0(sizeof(VSockUserConnection));
    conn->sioc = QIO_CHANNEL_SOCKET(ioc);
    conn->vsock = vsock;
    conn->state = VSOCK_USER_CONNECTION_STATE_CONNECTED_HOST;
    fifo8_create(&conn->host_buf, BUF_ALLOC);
    qemu_mutex_init(&conn->host_buf_lock);
    conn->close_handler_watch_source_id = qio_channel_add_watch(QIO_CHANNEL(conn->sioc),
        G_IO_HUP, virtio_vsock_sock_closed, conn, NULL);
    
    qio_channel_set_aio_fd_handler(QIO_CHANNEL(conn->sioc),
        aio_ctx, virtio_vsock_sock_read_avaliable,
        aio_ctx, NULL,
        conn);
}

static void virtio_vsock_host_new_connection(QIOTask *task, gpointer opaque)
{
    // TODO: add timeout
    VSockUserConnection* conn = opaque;
    IOThread* io_thread = conn->vsock->io_thread;
    AioContext* aio_ctx = iothread_get_aio_context(io_thread);
    ssize_t bytes_written = -1;
    Error* err = NULL;

    if(conn->sioc->fd == -1) {
        aio_bh_schedule_oneshot(qemu_get_aio_context(), 
            (void (*)(void *))virtio_vsock_conn_rst, opaque);
        return;
    }

    bytes_written = qio_channel_write(QIO_CHANNEL(conn->sioc),
        (void*)conn->key, sizeof(VSockUserConnectionKey), &err); 
    if(bytes_written != sizeof(VSockUserConnectionKey)) {
        error_report("virtio-vsock: "
            "Failed to send VSockUser connection request to the host. %s",
            error_get_pretty(err));
        g_free(err);
        aio_bh_schedule_oneshot(qemu_get_aio_context(),
            (void (*)(void *))virtio_vsock_conn_rst, opaque);
        return;
    }

    if(!qio_channel_set_blocking(QIO_CHANNEL(conn->sioc), false, &err)) {
        error_report("virtio-vsock: "
            "Failed to set new guest-sock connection to non-blocking mode. %s",
            error_get_pretty(err));
        g_free(err);
        aio_bh_schedule_oneshot(qemu_get_aio_context(),
            (void (*)(void *))virtio_vsock_conn_rst, opaque);
        return;
    }

    qio_channel_set_aio_fd_handler(QIO_CHANNEL(conn->sioc), aio_ctx,
        virtio_vsock_sock_read_avaliable, aio_ctx, NULL, conn);
    conn->close_handler_watch_source_id = qio_channel_add_watch(QIO_CHANNEL(conn->sioc),
        G_IO_HUP, virtio_vsock_sock_closed, conn, NULL);
}

static void virtio_vsock_vm_connection_request(VirtIOVSock *vsock, struct virtio_vsock_hdr *hdr)
{
    VSockUserConnectionKey* key = g_malloc0(sizeof(VSockUserConnectionKey));
    VSockUserConnection* conn = g_malloc0(sizeof(VSockUserConnection));

    virtio_vsock_init_conn_key(key, hdr);
    conn->vsock = vsock;
    conn->key = key;
    conn->vm_fwd_cnt = le32_to_cpu(hdr->fwd_cnt);
    conn->vm_buf_alloc = le32_to_cpu(hdr->buf_alloc);
    conn->state = VSOCK_USER_CONNECTION_STATE_CONNECTED_VM;
    fifo8_create(&conn->host_buf, BUF_ALLOC);
    QIOChannelSocket* sioc = conn->sioc = qio_channel_socket_new();
    qemu_mutex_init(&conn->host_buf_lock);

    qio_channel_socket_connect_async(sioc, &vsock->host_uds_addr,
        virtio_vsock_host_new_connection, conn, NULL, NULL);
    g_hash_table_insert(vsock->connection_hash_table, key, conn);
}

static void virtio_vsock_vm_receive(VSockUserConnection* conn,
                               VirtQueueElement *tx_elem, uint32_t len)
{
    VirtIOVSock* vsock = conn->vsock;
    uint8_t buf[BUF_ALLOC];

    qemu_mutex_lock(&conn->host_buf_lock);

    if(fifo8_num_free(&conn->host_buf) < len) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: guest pushed too much data for us to handle");
        return;
    }

    if(iov_to_buf(tx_elem->out_sg, tx_elem->out_num, VSOCK_HDR_LEN, &buf, len) != len) {
        virtio_error(VIRTIO_DEVICE(vsock),
            "virtio-vsock: tx VirtQueueElement's out buffer is too small!");
        // element is pushed back and freed in virtio_vsock_handle_tx_vq()
        return;
    }

    fifo8_push_all(&conn->host_buf, buf, len);

    // add write handler
    AioContext* aio_ctx = iothread_get_aio_context(vsock->io_thread);
    qio_channel_set_aio_fd_handler(QIO_CHANNEL(conn->sioc),
        aio_ctx, virtio_vsock_sock_read_avaliable,
        aio_ctx, virtio_vsock_sock_write_avaliable,
        conn); 
    qemu_mutex_unlock(&conn->host_buf_lock);
}

static void virtio_vsock_vm_connection_rst(VSockUserConnection* conn)
{
    g_hash_table_remove(conn->vsock->connection_hash_table, conn->key);
}

static void virtio_vsock_vm_connection_shutdown(VSockUserConnection* conn, uint32_t flags)
{
    bool shutdown_rcv_state = 0;
    bool shutdown_send_state = 0;

    if((flags & VIRTIO_VSOCK_SHUTDOWN_RCV) == VIRTIO_VSOCK_SHUTDOWN_RCV) {
        conn->state |= VSOCK_USER_CONNECTION_STATE_SHUTDOWN_RCV;
    }
    if((flags & VIRTIO_VSOCK_SHUTDOWN_SEND) == VIRTIO_VSOCK_SHUTDOWN_SEND) {
        conn->state |= VSOCK_USER_CONNECTION_STATE_SHUTDOWN_SEND;
    }

    shutdown_rcv_state = conn->state & VSOCK_USER_CONNECTION_STATE_SHUTDOWN_RCV;
    shutdown_send_state = conn->state & VSOCK_USER_CONNECTION_STATE_SHUTDOWN_SEND;

    if(shutdown_rcv_state && shutdown_send_state) {
        virtio_vsock_conn_rst(conn);
    }
}

static void virtio_vsock_do_handle_tx_vq(VirtIODevice *vdev, VirtQueue *vq)
{
    VirtIOVSock* vsock = VIRTIO_VSOCK(vdev);
    VirtQueueElement *elem = NULL;
    VSockUserConnectionKey key = { 0 };
    VSockUserConnection* conn = NULL;

    elem = virtqueue_pop(vq, sizeof(VirtQueueElement));
    if(!elem) {
        return;
    }

    struct virtio_vsock_hdr hdr;
    if(iov_to_buf(elem->out_sg, elem->out_num, 0, &hdr, VSOCK_HDR_LEN) != VSOCK_HDR_LEN) {
        virtio_error(vdev,
            "virtio-vsock: tx VirtQueueElement's out buffer is too small!");
        virtqueue_detach_element(vq, elem, 0);
        g_free(elem);
        return;
    }

    virtio_vsock_init_conn_key(&key, &hdr);
    conn = g_hash_table_lookup(vsock->connection_hash_table, &key);
    if(conn == NULL && hdr.op != VIRTIO_VSOCK_OP_REQUEST) {
        VSockUserConnection fake_conn = { 0 };
        fake_conn.vsock = vsock;
        fake_conn.key = &key;
        virtio_vsock_conn_rst(&fake_conn);
        goto out;
    }

    if(le32_to_cpu(hdr.type) != VIRTIO_VSOCK_TYPE_STREAM) {
        virtio_vsock_conn_rst(conn);
        goto out;
    }

    uint16_t op = le16_to_cpu(hdr.op);
    switch (op) {
    case VIRTIO_VSOCK_OP_REQUEST:
        virtio_vsock_vm_connection_request(vsock, &hdr);
        break;
    case VIRTIO_VSOCK_OP_RESPONSE:
        conn->state = VSOCK_USER_CONNECTION_STATE_CONNECTED;
        break;
    case VIRTIO_VSOCK_OP_RST:
        virtio_vsock_vm_connection_rst(conn);
        break;
    case VIRTIO_VSOCK_OP_SHUTDOWN:
        virtio_vsock_vm_connection_shutdown(conn, le32_to_cpu(hdr.flags));
        break;
    case VIRTIO_VSOCK_OP_RW:
        virtio_vsock_vm_receive(conn, elem, le32_to_cpu(hdr.len));
        break;
    case VIRTIO_VSOCK_OP_CREDIT_UPDATE:
        conn->vm_buf_alloc = hdr.buf_alloc;
        conn->vm_fwd_cnt = hdr.fwd_cnt;
        break;
    case VIRTIO_VSOCK_OP_CREDIT_REQUEST:
        virtio_vsock_conn_credit_update(conn);
        break;
    default:
        error_report("virtio-vsock: recived header with unknown OP (%hu)", op);
        virtio_vsock_conn_rst(conn);
        break;
    }

    out:
    virtqueue_push(vq, elem, 0);
    g_free(elem);
    virtio_notify(vdev, vq);
}

static void virtio_vsock_handle_tx_vq(VirtIODevice *vdev, VirtQueue *vq)
{
    while(!virtio_queue_empty(vq)) {
        virtio_vsock_do_handle_tx_vq(vdev, vq);
    }
}

static void virtio_vsock_get_config(VirtIODevice *vdev, uint8_t *config_data)
{
    VirtIOVSock *vsock = VIRTIO_VSOCK(vdev);
    struct virtio_vsock_config config = {
        .guest_cid = cpu_to_le64(vsock->conf.cid)
    };

    memcpy(config_data, &config, sizeof(config));
}

static void virtio_vsock_device_realize(DeviceState *dev, Error **errp)
{
    VirtIODevice *vdev = VIRTIO_DEVICE(dev);
    VirtIOVSock *vsock = VIRTIO_VSOCK(dev);

    if(vsock->conf.cid <= 2) {
        error_setg(errp, "cid property must contain value greater than 2");
        return;
    }
    if(vsock->conf.guest_uds_path == NULL) {
        error_setg(errp, "guest-uds-path property must contain a valid path");
        return;
    }
    GStatBuf host_uds_sb = { 0 };
    g_stat(vsock->conf.host_uds_path, &host_uds_sb);
    if((host_uds_sb.st_mode & S_IFMT) != S_IFSOCK) {
        error_setg(errp, "host-uds-path property must contain a path to valid UDS server");
        return;
    }

    vsock->host_uds_addr = (SocketAddress){
        .type = SOCKET_ADDRESS_TYPE_UNIX,
        .u.q_unix.path = vsock->conf.host_uds_path
    };

    vsock->guest_uds_addr = (SocketAddress){
        .type = SOCKET_ADDRESS_TYPE_UNIX,
        .u.q_unix.path = vsock->conf.guest_uds_path
    };
    vsock->guest_socket_listener = qio_net_listener_new();
    if(qio_net_listener_open_sync(vsock->guest_socket_listener,
                                  &vsock->guest_uds_addr, 128, errp) < 0)
    {
        return;
    }
    qio_net_listener_set_client_func(vsock->guest_socket_listener,
        virtio_vsock_guest_new_connection, vsock, NULL);

    vsock->connection_hash_table = g_hash_table_new_full(cht_key_hash,
        cht_key_cmp, NULL, cht_destroy_conn);
    if((vsock->io_thread = iothread_create("vsock_user_iothread", errp)) == NULL) {
        return;
    }

    virtio_init(vdev, VIRTIO_ID_VSOCK, sizeof(struct virtio_vsock_config));
    vsock->rx_vq = virtio_add_queue(vdev, 128, NULL);
    vsock->tx_vq = virtio_add_queue(vdev, 128, virtio_vsock_handle_tx_vq);
    vsock->event_vq = virtio_add_queue(vdev, 128, NULL);
}

static uint64_t virtio_vsock_get_features(VirtIODevice *vdev, uint64_t features,
                                          Error **errp)
{
    return features;
}

static void virtio_vsock_device_unrealize(DeviceState *dev)
{
    VirtIOVSock *vsock = VIRTIO_VSOCK(dev);

    qio_net_listener_disconnect(vsock->guest_socket_listener);
    iothread_stop(vsock->io_thread);
    g_hash_table_destroy(vsock->connection_hash_table);
    g_free(vsock->guest_socket_listener);
    g_free(vsock->io_thread);

    virtio_delete_queue(vsock->rx_vq);
    virtio_delete_queue(vsock->tx_vq);
    virtio_delete_queue(vsock->event_vq);
    virtio_cleanup(VIRTIO_DEVICE(dev));
}

static const Property virtio_vsock_properties[] = {
    DEFINE_PROP_UNSIGNED_NODEFAULT("cid", VirtIOVSock, conf.cid, qdev_prop_uint32, uint32_t),
    DEFINE_PROP_STRING("host-uds-path", VirtIOVSock, conf.host_uds_path),
    DEFINE_PROP_STRING("guest-uds-path", VirtIOVSock, conf.guest_uds_path),
};

static void virtio_vsock_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);
    device_class_set_props(dc, virtio_vsock_properties);

    VirtioDeviceClass *vdc = VIRTIO_DEVICE_CLASS(klass);
    vdc->realize = virtio_vsock_device_realize;
    vdc->unrealize = virtio_vsock_device_unrealize;
    vdc->get_features = virtio_vsock_get_features;
    vdc->get_config = virtio_vsock_get_config;
}

static const TypeInfo virtio_vsock_info = {
    .name = TYPE_VIRTIO_VSOCK,
    .parent = TYPE_VIRTIO_DEVICE,
    .instance_size = sizeof(VirtIOVSock),
    .class_init = virtio_vsock_class_init,
};

static void virtio_vsock_register_types(void)
{
    type_register_static(&virtio_vsock_info);
}

type_init(virtio_vsock_register_types)