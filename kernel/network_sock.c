/******************************************************************************
 * torOS - Terminal Operating System
 * Network Socket API - Berkeley Sockets
 *
 * Copyright (c) 2025 torOS Contributors
 * License: MIT
 ******************************************************************************/

#include "../include/toros.h"
#include "../include/network.h"
#include "../include/virtio.h"

/* ===== Socket State Management ===== */

static socket_t socket_table[SOCKET_MAX];
static int socket_table_inited = 0;

void socket_table_init(void)
{
    if (socket_table_inited) return;
    for (int i = 0; i < SOCKET_MAX; i++) {
        socket_table[i].state = SOCK_STATE_UNUSED;
        socket_table[i].fd = -1;
        socket_table[i].type = 0;
        socket_table[i].rx_buffer = NULL;
        socket_table[i].tx_buffer = NULL;
    }
    socket_table_inited = 1;
}

static int alloc_socket(void)
{
    for (int i = 0; i < SOCKET_MAX; i++) {
        if (socket_table[i].state == SOCK_STATE_UNUSED) {
            socket_table[i].fd = i;
            return i;
        }
    }
    return -1;
}

static void free_socket(int sockfd)
{
    if (sockfd < 0 || sockfd >= SOCKET_MAX) return;
    socket_t *s = &socket_table[sockfd];
    if (s->rx_buffer) { kfree(s->rx_buffer); s->rx_buffer = NULL; }
    if (s->tx_buffer) { kfree(s->tx_buffer); s->tx_buffer = NULL; }
    memset(s, 0, sizeof(socket_t));
    s->state = SOCK_STATE_UNUSED;
    s->fd = -1;
}

/* ===== Berkeley Socket API ===== */

int socket(int domain, int type, int protocol)
{
    if (!socket_table_inited) socket_table_init();
    if (domain != AF_INET && domain != AF_INET6) {
        printk_color(TERM_RED, "[SOCKET] Unsupported domain: %d\n", domain);
        return -1;
    }
    if (type != SOCK_STREAM && type != SOCK_DGRAM) {
        printk_color(TERM_RED, "[SOCKET] Unsupported type: %d\n", type);
        return -1;
    }

    int fd = alloc_socket();
    if (fd < 0) {
        printk_color(TERM_RED, "[SOCKET] No free socket slots\n");
        return -1;
    }

    socket_t *s = &socket_table[fd];
    s->domain = domain;
    s->type = type;
    s->protocol = protocol;
    s->state = SOCK_STATE_CLOSED;
    s->rx_buffer = (uint8 *)kmalloc(SOCKET_BUF_SIZE);
    s->tx_buffer = (uint8 *)kmalloc(SOCKET_BUF_SIZE);
    if (!s->rx_buffer || !s->tx_buffer) {
        free_socket(fd);
        return -1;
    }
    s->rx_head = s->rx_tail = 0;
    s->tx_head = s->tx_tail = 0;

    printk_color(TERM_GREEN, "[SOCKET] Created fd=%d (type=%s)\n", fd,
                 type == SOCK_STREAM ? "TCP" : "UDP");
    return fd;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    (void)addrlen;
    if (sockfd < 0 || sockfd >= SOCKET_MAX) return -1;
    socket_t *s = &socket_table[sockfd];
    if (s->state == SOCK_STATE_UNUSED) return -1;

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)addr;
        s->local_addr.addr = sin->sin_addr.s_addr;
        s->local_addr.port = ntohs(sin->sin_port);
        s->state = SOCK_STATE_BOUND;
        printk_color(TERM_GREEN, "[SOCKET] fd=%d bound to port %d\n", sockfd, s->local_addr.port);
        return 0;
    }
    return -1;
}

int listen(int sockfd, int backlog)
{
    if (sockfd < 0 || sockfd >= SOCKET_MAX) return -1;
    socket_t *s = &socket_table[sockfd];
    if (s->state != SOCK_STATE_BOUND || s->type != SOCK_STREAM) return -1;

    s->backlog = backlog > 0 ? backlog : 5;
    s->state = SOCK_STATE_LISTENING;
    printk_color(TERM_GREEN, "[SOCKET] fd=%d listening (backlog=%d)\n", sockfd, s->backlog);
    return 0;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    (void)addrlen;
    if (sockfd < 0 || sockfd >= SOCKET_MAX) return -1;
    socket_t *s = &socket_table[sockfd];
    if (s->state != SOCK_STATE_LISTENING) return -1;

    /* Wait for incoming connection */
    int timeout = 5000;
    while (timeout-- > 0) {
        uint8 packet[2048];
        int len = nic_recv(packet, sizeof(packet));
        if (len > 0) {
            eth_header_t *eth = (eth_header_t *)packet;
            if (ntohs(eth->type) == ETH_TYPE_IP) {
                ip_header_t *ip = (ip_header_t *)(packet + ETH_HDR_LEN);
                if (ip->protocol == IP_PROTO_TCP) {
                    tcp_header_t *tcp = (tcp_header_t *)((uint8 *)ip + IP_HDR_LEN(ip->version_ihl));
                    if (tcp->flags & TCP_SYN && !(tcp->flags & TCP_ACK)) {
                        /* New connection - create child socket */
                        int child_fd = socket(s->domain, s->type, s->protocol);
                        if (child_fd < 0) return -1;

                        socket_t *child = &socket_table[child_fd];
                        child->state = SOCK_STATE_ESTABLISHED;
                        child->remote_addr.addr = ip->src_ip;
                        child->remote_addr.port = ntohs(tcp->src_port);
                        child->local_addr = s->local_addr;

                        /* Send SYN-ACK */
                        tcp_send_synack(&child->local_addr, &child->remote_addr,
                                        ntohl(tcp->seq_num));

                        if (addr) {
                            struct sockaddr_in *sin = (struct sockaddr_in *)addr;
                            sin->sin_family = AF_INET;
                            sin->sin_addr.s_addr = ip->src_ip;
                            sin->sin_port = tcp->src_port;
                        }
                        printk_color(TERM_GREEN, "[SOCKET] fd=%d accepted connection from ", child_fd);
                        char ipbuf[16];
                        inet_ntoa(ip->src_ip, ipbuf);
                        printk("%s:%d\n", ipbuf, ntohs(tcp->src_port));
                        return child_fd;
                    }
                }
            }
        }
        rtc_mdelay(1);
    }
    return -1;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    (void)addrlen;
    if (sockfd < 0 || sockfd >= SOCKET_MAX) return -1;
    socket_t *s = &socket_table[sockfd];
    if (s->state == SOCK_STATE_UNUSED) return -1;

    if (addr->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)addr;
        s->remote_addr.addr = sin->sin_addr.s_addr;
        s->remote_addr.port = ntohs(sin->sin_port);

        if (s->type == SOCK_STREAM) {
            /* TCP: Send SYN */
            s->local_seq = (uint32)rtc_get_ticks();
            s->state = SOCK_STATE_SYN_SENT;

            tcp_header_t syn;
            memset(&syn, 0, sizeof(syn));
            syn.src_port = htons(s->local_addr.port ? s->local_addr.port : alloc_ephemeral_port());
            syn.dst_port = htons(s->remote_addr.port);
            syn.seq_num = htonl(s->local_seq);
            syn.ack_num = 0;
            syn.data_offset = (20 / 4) << 4;
            syn.flags = TCP_SYN;
            syn.window = htons(65535);
            syn.checksum = 0;
            syn.urgent = 0;

            uint8 packet[40];
            memcpy(packet, &syn, 20);
            ip_send(s->remote_addr.addr, IP_PROTO_TCP, packet, 20);

            /* Wait for SYN-ACK */
            int timeout = 3000;
            while (timeout-- > 0) {
                uint8 rx_pkt[2048];
                int len = nic_recv(rx_pkt, sizeof(rx_pkt));
                if (len > 0) {
                    eth_header_t *eth = (eth_header_t *)rx_pkt;
                    if (ntohs(eth->type) == ETH_TYPE_IP) {
                        ip_header_t *ip = (ip_header_t *)(rx_pkt + ETH_HDR_LEN);
                        if (ip->protocol == IP_PROTO_TCP && ip->src_ip == s->remote_addr.addr) {
                            tcp_header_t *rx_tcp = (tcp_header_t *)((uint8 *)ip + IP_HDR_LEN(ip->version_ihl));
                            if ((rx_tcp->flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                                /* Send ACK */
                                s->remote_seq = ntohl(rx_tcp->seq_num);
                                tcp_header_t ack;
                                memset(&ack, 0, sizeof(ack));
                                ack.src_port = syn.src_port;
                                ack.dst_port = htons(s->remote_addr.port);
                                ack.seq_num = htonl(s->local_seq + 1);
                                ack.ack_num = htonl(s->remote_seq + 1);
                                ack.data_offset = (20 / 4) << 4;
                                ack.flags = TCP_ACK;
                                ack.window = htons(65535);

                                uint8 ack_pkt[40];
                                memcpy(ack_pkt, &ack, 20);
                                ip_send(s->remote_addr.addr, IP_PROTO_TCP, ack_pkt, 20);

                                s->local_addr.port = ntohs(syn.src_port);
                                s->state = SOCK_STATE_ESTABLISHED;
                                printk_color(TERM_GREEN, "[SOCKET] fd=%d connected\n", sockfd);
                                return 0;
                            }
                        }
                    }
                }
                rtc_mdelay(1);
            }
            s->state = SOCK_STATE_CLOSED;
            return -1;
        } else {
            /* UDP: Stateless */
            if (s->local_addr.port == 0)
                s->local_addr.port = alloc_ephemeral_port();
            s->state = SOCK_STATE_ESTABLISHED;
            return 0;
        }
    }
    return -1;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags)
{
    (void)flags;
    if (sockfd < 0 || sockfd >= SOCKET_MAX || !buf || len == 0) return -1;
    socket_t *s = &socket_table[sockfd];
    if (s->state != SOCK_STATE_ESTABLISHED) return -1;

    if (s->type == SOCK_STREAM) {
        /* TCP segment */
        uint8 packet[20 + len];
        tcp_header_t *tcp = (tcp_header_t *)packet;
        tcp->src_port = htons(s->local_addr.port);
        tcp->dst_port = htons(s->remote_addr.port);
        tcp->seq_num = htonl(s->local_seq);
        tcp->ack_num = htonl(s->remote_seq + 1);
        tcp->data_offset = (20 / 4) << 4;
        tcp->flags = TCP_PSH | TCP_ACK;
        tcp->window = htons(65535);
        tcp->checksum = 0;
        tcp->urgent = 0;
        memcpy(packet + 20, buf, len);
        tcp->checksum = ip_checksum(packet, 20 + len);

        ip_send(s->remote_addr.addr, IP_PROTO_TCP, packet, 20 + len);
        s->local_seq += len;
        return len;
    } else {
        /* UDP datagram */
        uint8 packet[8 + len];
        udp_header_t *udp = (udp_header_t *)packet;
        udp->src_port = htons(s->local_addr.port);
        udp->dst_port = htons(s->remote_addr.port);
        udp->length = htons(8 + len);
        udp->checksum = 0;
        memcpy(packet + 8, buf, len);

        ip_send(s->remote_addr.addr, IP_PROTO_UDP, packet, 8 + len);
        return len;
    }
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags)
{
    (void)flags;
    if (sockfd < 0 || sockfd >= SOCKET_MAX || !buf) return -1;
    socket_t *s = &socket_table[sockfd];
    if (s->state != SOCK_STATE_ESTABLISHED && s->state != SOCK_STATE_CLOSE_WAIT) return -1;

    /* Check RX buffer first */
    int avail = s->rx_tail - s->rx_head;
    if (avail < 0) avail += SOCKET_BUF_SIZE;
    if (avail > 0) {
        int to_read = (avail < (int)len) ? avail : (int)len;
        for (int i = 0; i < to_read; i++) {
            ((uint8 *)buf)[i] = s->rx_buffer[s->rx_head];
            s->rx_head = (s->rx_head + 1) % SOCKET_BUF_SIZE;
        }
        return to_read;
    }

    /* Poll for incoming data */
    int timeout = 100;
    while (timeout-- > 0) {
        uint8 packet[2048];
        int pkt_len = nic_recv(packet, sizeof(packet));
        if (pkt_len > 0) {
            eth_header_t *eth = (eth_header_t *)packet;
            if (ntohs(eth->type) == ETH_TYPE_IP) {
                ip_header_t *ip = (ip_header_t *)(packet + ETH_HDR_LEN);
                uint8 ip_hdr_len = IP_HDR_LEN(ip->version_ihl);

                if (ip->protocol == IP_PROTO_TCP && s->type == SOCK_STREAM) {
                    tcp_header_t *tcp = (tcp_header_t *)((uint8 *)ip + ip_hdr_len);
                    if (ntohs(tcp->dst_port) == s->local_addr.port &&
                        ntohs(tcp->src_port) == s->remote_addr.port) {
                        uint8 tcp_hdr_len = TCP_HDR_LEN(tcp->data_offset);
                        uint32 payload_len = ntohs(ip->total_len) - ip_hdr_len - tcp_hdr_len;
                        if (payload_len > 0) {
                            uint8 *payload = (uint8 *)tcp + tcp_hdr_len;
                            uint32 copy_len = (payload_len > len) ? len : payload_len;
                            memcpy(buf, payload, copy_len);
                            s->remote_seq = ntohl(tcp->seq_num) + payload_len;
                            return copy_len;
                        }
                    }
                } else if (ip->protocol == IP_PROTO_UDP && s->type == SOCK_DGRAM) {
                    udp_header_t *udp = (udp_header_t *)((uint8 *)ip + ip_hdr_len);
                    if (ntohs(udp->dst_port) == s->local_addr.port) {
                        uint32 payload_len = ntohs(udp->length) - 8;
                        uint8 *payload = (uint8 *)udp + 8;
                        uint32 copy_len = (payload_len > len) ? len : payload_len;
                        memcpy(buf, payload, copy_len);
                        return copy_len;
                    }
                }
            }
        }
        rtc_mdelay(10);
    }
    return 0;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen)
{
    (void)flags;
    (void)addrlen;
    if (sockfd < 0 || sockfd >= SOCKET_MAX || !buf || len == 0 || !dest_addr) return -1;
    socket_t *s = &socket_table[sockfd];

    if (dest_addr->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)dest_addr;
        uint32 dst_ip = sin->sin_addr.s_addr;
        uint16 dst_port = ntohs(sin->sin_port);

        uint8 packet[8 + len];
        udp_header_t *udp = (udp_header_t *)packet;
        udp->src_port = htons(s->local_addr.port ? s->local_addr.port : alloc_ephemeral_port());
        udp->dst_port = htons(dst_port);
        udp->length = htons(8 + len);
        udp->checksum = 0;
        memcpy(packet + 8, buf, len);

        ip_send(dst_ip, IP_PROTO_UDP, packet, 8 + len);
        return len;
    }
    return -1;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen)
{
    (void)flags;
    (void)addrlen;
    if (sockfd < 0 || sockfd >= SOCKET_MAX || !buf) return -1;

    int timeout = 100;
    while (timeout-- > 0) {
        uint8 packet[2048];
        int pkt_len = nic_recv(packet, sizeof(packet));
        if (pkt_len > 0) {
            eth_header_t *eth = (eth_header_t *)packet;
            if (ntohs(eth->type) == ETH_TYPE_IP) {
                ip_header_t *ip = (ip_header_t *)(packet + ETH_HDR_LEN);
                if (ip->protocol == IP_PROTO_UDP) {
                    udp_header_t *udp = (udp_header_t *)((uint8 *)ip + IP_HDR_LEN(ip->version_ihl));
                    uint32 payload_len = ntohs(udp->length) - 8;
                    uint8 *payload = (uint8 *)udp + 8;
                    uint32 copy_len = (payload_len > len) ? len : payload_len;
                    memcpy(buf, payload, copy_len);

                    if (src_addr) {
                        struct sockaddr_in *sin = (struct sockaddr_in *)src_addr;
                        sin->sin_family = AF_INET;
                        sin->sin_addr.s_addr = ip->src_ip;
                        sin->sin_port = udp->src_port;
                    }
                    return copy_len;
                }
            }
        }
        rtc_mdelay(10);
    }
    return 0;
}

int shutdown(int sockfd, int how)
{
    if (sockfd < 0 || sockfd >= SOCKET_MAX) return -1;
    socket_t *s = &socket_table[sockfd];
    if (s->state != SOCK_STATE_ESTABLISHED) return -1;

    if (how == SHUT_RD || how == SHUT_RDWR) {
        s->state = SOCK_STATE_CLOSE_WAIT;
    }
    if (how == SHUT_WR || how == SHUT_RDWR) {
        /* Send FIN */
        tcp_header_t fin;
        memset(&fin, 0, sizeof(fin));
        fin.src_port = htons(s->local_addr.port);
        fin.dst_port = htons(s->remote_addr.port);
        fin.seq_num = htonl(s->local_seq);
        fin.ack_num = htonl(s->remote_seq + 1);
        fin.data_offset = (20 / 4) << 4;
        fin.flags = TCP_FIN | TCP_ACK;
        fin.window = htons(0);

        uint8 packet[40];
        memcpy(packet, &fin, 20);
        ip_send(s->remote_addr.addr, IP_PROTO_TCP, packet, 20);
    }
    return 0;
}

int closesocket(int sockfd)
{
    if (sockfd < 0 || sockfd >= SOCKET_MAX) return -1;
    socket_t *s = &socket_table[sockfd];
    if (s->state == SOCK_STATE_ESTABLISHED && s->type == SOCK_STREAM) {
        shutdown(sockfd, SHUT_RDWR);
    }
    free_socket(sockfd);
    printk_color(TERM_GREEN, "[SOCKET] fd=%d closed\n", sockfd);
    return 0;
}

/* ===== Epoll-like Interface ===== */

int epoll_create(int size)
{
    (void)size;
    printk_color(TERM_YELLOW, "[SOCKET] epoll_create stub\n");
    return 0;
}

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
    (void)epfd;
    (void)op;
    (void)fd;
    (void)event;
    return 0;
}

int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
    (void)epfd;
    (void)events;
    (void)maxevents;
    rtc_mdelay(timeout);
    return 0;
}

/* ===== Helper Functions ===== */

uint16 alloc_ephemeral_port(void)
{
    static uint16 next_port = 49152;
    return next_port++;
}

void tcp_send_synack(ip_addr_t *src, ip_addr_t *dst, uint32 seq)
{
    tcp_header_t synack;
    memset(&synack, 0, sizeof(synack));
    synack.src_port = htons(src->port);
    synack.dst_port = htons(dst->port);
    synack.seq_num = htonl((uint32)rtc_get_ticks());
    synack.ack_num = htonl(seq + 1);
    synack.data_offset = (20 / 4) << 4;
    synack.flags = TCP_SYN | TCP_ACK;
    synack.window = htons(65535);
    synack.checksum = 0;

    uint8 packet[40];
    memcpy(packet, &synack, 20);
    ip_send(dst->addr, IP_PROTO_TCP, packet, 20);
}

void tcp_periodic_all(void)
{
    for (int i = 0; i < SOCKET_MAX; i++) {
        socket_t *s = &socket_table[i];
        if (s->state == SOCK_STATE_SYN_SENT) {
            /* Retransmit SYN if needed */
        } else if (s->state == SOCK_STATE_ESTABLISHED) {
            /* Check for idle timeout */
        }
    }
}
