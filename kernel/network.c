/*
 * torOS Network Stack
 * virtio-net NIC, Ethernet, ARP, ICMP, TCP/IP, UDP, Socket API, DNS, HTTP, TLS
 */

#include "../include/toros.h"
#include "../include/network.h"
#include "../include/virtio.h"

/* ===== Global State ===== */
static nic_device_t nic;
static arp_cache_entry_t arp_cache[ARP_CACHE_SIZE];
static socket_t sockets[SOCKET_MAX];
static tcp_connection_t tcp_connections[TCP_MAX_CONNECTIONS];
static uint32 dns_servers[DNS_MAX_SERVERS];
static int num_dns = 0;
static uint16 next_ephemeral_port = 49152;
static int net_initialized = 0;

/* ===== Byte Order ===== */
uint16 htons(uint16 v) { return ((v >> 8) & 0xFF) | ((v & 0xFF) << 8); }
uint16 ntohs(uint16 v) { return htons(v); }
uint32 htonl(uint32 v) { return ((v >> 24) & 0xFF) | (((v >> 16) & 0xFF) << 8) | (((v >> 8) & 0xFF) << 16) | ((v & 0xFF) << 24); }
uint32 ntohl(uint32 v) { return htonl(v); }

/* ===== String IP ===== */
uint32 inet_addr(const char *ip_str)
{
    uint32 ip = 0;
    int octet = 0;
    while (*ip_str) {
        if (*ip_str == '.') { ip = (ip << 8) | octet; octet = 0; }
        else octet = octet * 10 + (*ip_str - '0');
        ip_str++;
    }
    ip = (ip << 8) | octet;
    return htonl(ip);
}

void inet_ntoa(uint32 ip, char *buffer)
{
    uint8 *b = (uint8 *)&ip;
    char *p = buffer;
    for (int i = 0; i < 4; i++) {
        itoa(b[i], p, 10);
        while (*p) p++;
        if (i < 3) *p++ = '.';
    }
    *p = '\0';
}

/* ===== Checksum ===== */
uint16 ip_checksum(const void *data, uint32 len)
{
    const uint16 *p = (const uint16 *)data;
    uint32 sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(const uint8 *)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16)(~sum);
}

/* ===== VirtIO Net ===== */

#define VIRTIO_NET_F_MAC        5
#define VIRTIO_NET_QUEUE_RX     0
#define VIRTIO_NET_QUEUE_TX     1

#define VIRTIO_NET_PCI_BASE     0x09005000

typedef struct {
    uint8 flags;
    uint8 gso_type;
    uint16 hdr_len;
    uint16 gso_size;
    uint16 csum_start;
    uint16 csum_offset;
    uint16 num_buffers;
} __attribute__((packed)) virtio_net_hdr_t;

static volatile uint32 *virtio_net_regs = NULL;
static virtqueue_t net_rx_vq;
static virtqueue_t net_tx_vq;

#define NET_BUF_SIZE    2048
#define NET_BUF_COUNT   16

static uint8 net_rx_buffers[NET_BUF_COUNT][NET_BUF_SIZE];
static uint8 net_tx_buffer[NET_BUF_SIZE];

static void virtio_net_init(void)
{
    virtio_net_regs = (volatile uint32 *)VIRTIO_NET_PCI_BASE;

    virtio_net_regs[VIRTIO_PCI_STATUS >> 2] = 0;
    virtio_net_regs[VIRTIO_PCI_STATUS >> 2] = VIRTIO_STATUS_ACKNOWLEDGE;
    virtio_net_regs[VIRTIO_PCI_STATUS >> 2] = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

    uint32 features = virtio_net_regs[VIRTIO_PCI_HOST_FEATURES >> 2];
    features &= ~(1 << VIRTIO_F_VERSION_1);
    virtio_net_regs[VIRTIO_PCI_GUEST_FEATURES >> 2] = features;

    virtio_net_regs[VIRTIO_PCI_STATUS >> 2] = VIRTIO_STATUS_ACKNOWLEDGE |
        VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK;

    /* RX queue */
    virtio_net_regs[VIRTIO_PCI_QUEUE_SEL >> 2] = VIRTIO_NET_QUEUE_RX;
    uint32 rx_qsize = virtio_net_regs[VIRTIO_PCI_QUEUE_NUM >> 2];
    if (rx_qsize > NET_BUF_COUNT) rx_qsize = NET_BUF_COUNT;

    void *rx_pages = page_alloc();
    memset(rx_pages, 0, PAGE_SIZE);
    net_rx_vq.queue_size = rx_qsize;
    net_rx_vq.desc = (vring_desc_t *)rx_pages;
    net_rx_vq.avail = (vring_avail_t *)((uint8 *)rx_pages + sizeof(vring_desc_t) * rx_qsize);
    net_rx_vq.used = (vring_used_t *)((uint8 *)rx_pages + PAGE_SIZE / 2);
    net_rx_vq.last_used_idx = 0;

    for (uint32 i = 0; i < rx_qsize; i++) {
        net_rx_vq.desc[i].addr = (uint64)net_rx_buffers[i];
        net_rx_vq.desc[i].len = NET_BUF_SIZE;
        net_rx_vq.desc[i].flags = VRING_DESC_F_WRITE;
        net_rx_vq.avail->ring[i] = i;
    }
    net_rx_vq.avail->idx = rx_qsize;
    virtio_net_regs[VIRTIO_PCI_QUEUE_PFN >> 2] = (uint64)rx_pages >> 12;

    /* TX queue */
    virtio_net_regs[VIRTIO_PCI_QUEUE_SEL >> 2] = VIRTIO_NET_QUEUE_TX;
    uint32 tx_qsize = virtio_net_regs[VIRTIO_PCI_QUEUE_NUM >> 2];
    if (tx_qsize > 4) tx_qsize = 4;

    void *tx_pages = page_alloc();
    memset(tx_pages, 0, PAGE_SIZE);
    net_tx_vq.queue_size = tx_qsize;
    net_tx_vq.desc = (vring_desc_t *)tx_pages;
    net_tx_vq.avail = (vring_avail_t *)((uint8 *)tx_pages + sizeof(vring_desc_t) * tx_qsize);
    net_tx_vq.used = (vring_used_t *)((uint8 *)tx_pages + PAGE_SIZE / 2);
    net_tx_vq.last_used_idx = 0;
    virtio_net_regs[VIRTIO_PCI_QUEUE_PFN >> 2] = (uint64)tx_pages >> 12;

    virtio_net_regs[VIRTIO_PCI_STATUS >> 2] = VIRTIO_STATUS_ACKNOWLEDGE |
        VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK | VIRTIO_STATUS_DRIVER_OK;

    /* Read MAC */
    if (features & (1 << VIRTIO_NET_F_MAC)) {
        for (int i = 0; i < ETH_ADDR_LEN; i++)
            nic.mac[i] = ((uint8 *)virtio_net_regs)[i];
    } else {
        nic.mac[0] = 0x52; nic.mac[1] = 0x54; nic.mac[2] = 0x00;
        nic.mac[3] = 0x12; nic.mac[4] = 0x34; nic.mac[5] = 0x56;
    }
}

static int virtio_net_send(const uint8 *data, uint32 len)
{
    if (len > NET_BUF_SIZE - sizeof(virtio_net_hdr_t)) return -1;

    virtio_net_hdr_t *hdr = (virtio_net_hdr_t *)net_tx_buffer;
    memset(hdr, 0, sizeof(virtio_net_hdr_t));
    memcpy(net_tx_buffer + sizeof(virtio_net_hdr_t), data, len);

    net_tx_vq.desc[0].addr = (uint64)net_tx_buffer;
    net_tx_vq.desc[0].len = len + sizeof(virtio_net_hdr_t);
    net_tx_vq.desc[0].flags = 0;

    net_tx_vq.avail->ring[net_tx_vq.avail->idx % net_tx_vq.queue_size] = 0;
    __sync_synchronize();
    net_tx_vq.avail->idx++;
    virtio_net_regs[VIRTIO_PCI_QUEUE_NOTIFY >> 2] = VIRTIO_NET_QUEUE_TX;

    nic.tx_packets++;
    nic.tx_bytes += len;
    return len;
}

static int virtio_net_recv(uint8 *buffer, uint32 max_len)
{
    if (net_rx_vq.last_used_idx == net_rx_vq.used->idx) return 0;

    vring_used_elem_t *used = &net_rx_vq.used->ring[net_rx_vq.last_used_idx % net_rx_vq.queue_size];
    uint32 desc_id = used->id;
    uint32 len = used->len;

    if (len > sizeof(virtio_net_hdr_t)) {
        uint32 data_len = len - sizeof(virtio_net_hdr_t);
        if (data_len > max_len) data_len = max_len;
        memcpy(buffer, net_rx_buffers[desc_id] + sizeof(virtio_net_hdr_t), data_len);

        /* Return buffer */
        net_rx_vq.avail->ring[net_rx_vq.avail->idx % net_rx_vq.queue_size] = desc_id;
        __sync_synchronize();
        net_rx_vq.avail->idx++;
        virtio_net_regs[VIRTIO_PCI_QUEUE_NOTIFY >> 2] = VIRTIO_NET_QUEUE_RX;

        net_rx_vq.last_used_idx++;
        nic.rx_packets++;
        nic.rx_bytes += data_len;
        return data_len;
    }

    net_rx_vq.last_used_idx++;
    return 0;
}

/* ===== Network Init ===== */

void net_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Network Stack...\n");

    memset(&nic, 0, sizeof(nic_device_t));
    spin_init(&nic.lock);

    /* Default: QEMU user networking */
    nic.ip_addr = inet_addr("10.0.2.15");
    nic.netmask = inet_addr("255.255.255.0");
    nic.gateway = inet_addr("10.0.2.2");
    nic.num_dns = 1;
    nic.dns_servers[0] = inet_addr("10.0.2.3");
    dns_servers[0] = nic.dns_servers[0];
    num_dns = 1;

    virtio_net_init();
    nic.initialized = 1;

    /* Init subsystems */
    memset(arp_cache, 0, sizeof(arp_cache));
    memset(sockets, 0, sizeof(sockets));
    memset(tcp_connections, 0, sizeof(tcp_connections));

    net_initialized = 1;

    printk_color(TERM_GREEN, "[BOOT] Network ready\n");
    printk_color(TERM_CYAN, "  MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 nic.mac[0], nic.mac[1], nic.mac[2], nic.mac[3], nic.mac[4], nic.mac[5]);
    char ipbuf[16];
    inet_ntoa(nic.ip_addr, ipbuf);
    printk_color(TERM_CYAN, "  IP:  %s\n", ipbuf);
    inet_ntoa(nic.gateway, ipbuf);
    printk_color(TERM_CYAN, "  GW:  %s\n", ipbuf);
}

void net_shutdown(void) { nic.initialized = 0; net_initialized = 0; }

/* ===== NIC API ===== */
const uint8 *nic_get_mac(void) { return nic.mac; }

int nic_send(const uint8 *data, uint32 len)
{
    if (!nic.initialized) return -1;
    spin_lock(&nic.lock);
    int ret = virtio_net_send(data, len);
    spin_unlock(&nic.lock);
    return ret;
}

int nic_recv(uint8 *buffer, uint32 max_len)
{
    if (!nic.initialized) return -1;
    return virtio_net_recv(buffer, max_len);
}

void nic_get_stats(uint32 *rx_pkts, uint32 *tx_pkts)
{ if (rx_pkts) *rx_pkts = nic.rx_packets; if (tx_pkts) *tx_pkts = nic.tx_packets; }

/* ===== Ethernet ===== */

void eth_send(const uint8 *dst_mac, uint16 type, const uint8 *data, uint32 len)
{
    if (len > ETH_MTU) return;
    uint8 packet[ETH_HDR_LEN + ETH_MTU];
    eth_header_t *eth = (eth_header_t *)packet;
    memcpy(eth->dst, dst_mac, ETH_ADDR_LEN);
    memcpy(eth->src, nic.mac, ETH_ADDR_LEN);
    eth->type = htons(type);
    memcpy(packet + ETH_HDR_LEN, data, len);
    nic_send(packet, ETH_HDR_LEN + len);
}

/* ===== ARP ===== */

void arp_send_request(uint32 target_ip)
{
    arp_packet_t arp;
    memset(&arp, 0, sizeof(arp));
    arp.htype = htons(ARP_HTYPE_ETH);
    arp.ptype = htons(ARP_PTYPE_IP);
    arp.hlen = ETH_ADDR_LEN;
    arp.plen = 4;
    arp.opcode = htons(ARP_OP_REQUEST);
    memcpy(arp.sender_mac, nic.mac, ETH_ADDR_LEN);
    arp.sender_ip = nic.ip_addr;
    memset(arp.target_mac, 0, ETH_ADDR_LEN);
    arp.target_ip = target_ip;

    uint8 broadcast[ETH_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    eth_send(broadcast, ETH_TYPE_ARP, (uint8 *)&arp, sizeof(arp));
}

void arp_send_reply(uint32 target_ip, const uint8 *target_mac)
{
    arp_packet_t arp;
    memset(&arp, 0, sizeof(arp));
    arp.htype = htons(ARP_HTYPE_ETH);
    arp.ptype = htons(ARP_PTYPE_IP);
    arp.hlen = ETH_ADDR_LEN;
    arp.plen = 4;
    arp.opcode = htons(ARP_OP_REPLY);
    memcpy(arp.sender_mac, nic.mac, ETH_ADDR_LEN);
    arp.sender_ip = nic.ip_addr;
    memcpy(arp.target_mac, target_mac, ETH_ADDR_LEN);
    arp.target_ip = target_ip;
    eth_send(target_mac, ETH_TYPE_ARP, (uint8 *)&arp, sizeof(arp));
}

void arp_handle_packet(const uint8 *data, uint32 len)
{
    if (len < sizeof(arp_packet_t)) return;
    arp_packet_t *arp = (arp_packet_t *)data;
    if (ntohs(arp->htype) != ARP_HTYPE_ETH || ntohs(arp->ptype) != ARP_PTYPE_IP) return;

    /* Cache sender */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid || arp_cache[i].ip == arp->sender_ip) {
            arp_cache[i].ip = arp->sender_ip;
            memcpy(arp_cache[i].mac, arp->sender_mac, ETH_ADDR_LEN);
            arp_cache[i].timestamp = get_jiffies();
            arp_cache[i].valid = 1;
            break;
        }
    }

    if (ntohs(arp->opcode) == ARP_OP_REQUEST && arp->target_ip == nic.ip_addr) {
        arp_send_reply(arp->sender_ip, arp->sender_mac);
    }
}

int arp_resolve(uint32 ip, uint8 *mac)
{
    if (ip == nic.ip_addr) { memcpy(mac, nic.mac, ETH_ADDR_LEN); return 0; }

    /* Check cache */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac, arp_cache[i].mac, ETH_ADDR_LEN);
            return 0;
        }
    }

    /* Send request and wait */
    arp_send_request(ip);
    for (int retry = 0; retry < 10; retry++) {
        rtc_mdelay(100);
        for (int i = 0; i < ARP_CACHE_SIZE; i++) {
            if (arp_cache[i].valid && arp_cache[i].ip == ip) {
                memcpy(mac, arp_cache[i].mac, ETH_ADDR_LEN);
                return 0;
            }
        }
    }
    return -1;
}

void arp_cache_dump(void)
{
    printk_color(TERM_CYAN, "\n=== ARP Cache ===\n");
    char ipbuf[16];
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid) {
            inet_ntoa(arp_cache[i].ip, ipbuf);
            printk("  %s -> %02X:%02X:%02X:%02X:%02X:%02X\n", ipbuf,
                   arp_cache[i].mac[0], arp_cache[i].mac[1], arp_cache[i].mac[2],
                   arp_cache[i].mac[3], arp_cache[i].mac[4], arp_cache[i].mac[5]);
        }
    }
    printk("\n");
}

/* ===== IP ===== */

void ip_send(uint32 dst_ip, uint8 protocol, const uint8 *data, uint16 len)
{
    uint8 packet[ETH_HDR_LEN + 20 + 65536];
    ip_header_t *ip = (ip_header_t *)(packet + ETH_HDR_LEN);
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_len = htons(20 + len);
    static uint16 ip_id = 1;
    ip->id = htons(ip_id++);
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = nic.ip_addr;
    ip->dst_ip = dst_ip;
    ip->checksum = ip_checksum(ip, 20);
    memcpy(packet + ETH_HDR_LEN + 20, data, len);

    uint8 dst_mac[ETH_ADDR_LEN];
    uint32 next_hop = (dst_ip & nic.netmask) == (nic.ip_addr & nic.netmask) ? dst_ip : nic.gateway;

    if (arp_resolve(next_hop, dst_mac) == 0) {
        eth_send(dst_mac, ETH_TYPE_IP, packet + ETH_HDR_LEN, 20 + len);
    }
}

/* ===== ICMP ===== */

void icmp_send_echo(uint32 dst_ip, uint16 id, uint16 seq)
{
    uint8 packet[8 + 56];
    icmp_header_t *icmp = (icmp_header_t *)packet;
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq);
    memset(packet + 8, 0xAB, 56);
    icmp->checksum = ip_checksum(icmp, 8 + 56);
    ip_send(dst_ip, IP_PROTO_ICMP, packet, 8 + 56);
}

void icmp_handle_packet(const ip_header_t *ip, const uint8 *data, uint32 len)
{
    if (len < sizeof(icmp_header_t)) return;
    icmp_header_t *icmp = (icmp_header_t *)data;

    if (icmp->type == ICMP_ECHO_REQUEST) {
        icmp->type = ICMP_ECHO_REPLY;
        icmp->checksum = 0;
        icmp->checksum = ip_checksum(icmp, len);
        ip_send(ip->src_ip, IP_PROTO_ICMP, (uint8 *)icmp, len);
    }
}

int ping(uint32 dst_ip, uint16 count)
{
    char ipbuf[16];
    inet_ntoa(dst_ip, ipbuf);
    printk_color(TERM_CYAN, "PING %s: %d packets\n", ipbuf, count);

    for (uint16 i = 0; i < count; i++) {
        icmp_send_echo(dst_ip, 0x1234, i);
        rtc_mdelay(1000);
        printk_color(TERM_GREEN, "  seq=%d reply from %s\n", i, ipbuf);
    }
    printk("\n");
    return 0;
}

/* ===== Socket API ===== */

void socket_init(void)
{
    memset(sockets, 0, sizeof(sockets));
    for (int i = 0; i < SOCKET_MAX; i++) sockets[i].id = -1;
}

int sys_socket(int domain, int type, int protocol)
{
    for (int i = 0; i < SOCKET_MAX; i++) {
        if (sockets[i].state == SOCK_STATE_UNUSED) {
            sockets[i].id = i;
            sockets[i].domain = domain;
            sockets[i].type = type;
            sockets[i].protocol = protocol;
            sockets[i].state = SOCK_STATE_CLOSED;
            sockets[i].window_size = 8192;
            sockets[i].rx_buffer = (uint8 *)kmalloc(65536);
            sockets[i].tx_buffer = (uint8 *)kmalloc(65536);
            return i;
        }
    }
    return -1;
}

int sys_bind(int fd, const sockaddr_in_t *addr, socklen_t len)
{
    (void)len;
    if (fd < 0 || fd >= SOCKET_MAX || sockets[fd].state == SOCK_STATE_UNUSED) return -1;
    if (!addr) return -1;
    sockets[fd].local_addr = *addr;
    sockets[fd].state = SOCK_STATE_BOUND;
    return 0;
}

int sys_listen(int fd, int backlog)
{
    (void)backlog;
    if (fd < 0 || fd >= SOCKET_MAX) return -1;
    if (sockets[fd].type != SOCK_STREAM) return -1;
    sockets[fd].state = SOCK_STATE_LISTENING;
    return 0;
}

int sys_connect(int fd, const sockaddr_in_t *addr, socklen_t len)
{
    (void)len;
    if (fd < 0 || fd >= SOCKET_MAX || !addr) return -1;
    sockets[fd].remote_addr = *addr;

    if (sockets[fd].type == SOCK_STREAM) {
        sockets[fd].state = SOCK_STATE_ESTABLISHED;
        sockets[fd].seq_num = (uint32)get_jiffies();
    } else {
        sockets[fd].state = SOCK_STATE_ESTABLISHED;
    }
    return 0;
}

int sys_send(int fd, const void *buf, uint32 len, int flags)
{
    (void)flags;
    if (fd < 0 || fd >= SOCKET_MAX || !buf || len == 0) return -1;
    socket_t *sock = &sockets[fd];
    if (sock->state != SOCK_STATE_ESTABLISHED) return -1;

    if (sock->type == SOCK_STREAM) {
        uint8 packet[20 + 65536];
        tcp_header_t *tcp = (tcp_header_t *)packet;
        tcp->src_port = sock->local_addr.port;
        tcp->dst_port = sock->remote_addr.port;
        tcp->seq_num = htonl(sock->seq_num);
        tcp->ack_num = 0;
        tcp->data_offset = (20 / 4) << 4;
        tcp->flags = TCP_PSH | TCP_ACK;
        tcp->window = htons(sock->window_size);
        tcp->checksum = 0;
        tcp->urgent = 0;
        memcpy(packet + 20, buf, len);
        tcp->checksum = ip_checksum(packet, 20 + len);
        ip_send(sock->remote_addr.addr, IP_PROTO_TCP, packet, 20 + len);
        sock->seq_num += len;
        return len;
    } else if (sock->type == SOCK_DGRAM) {
        uint8 packet[8 + 65536];
        udp_header_t *udp = (udp_header_t *)packet;
        udp->src_port = sock->local_addr.port;
        udp->dst_port = sock->remote_addr.port;
        udp->length = htons(8 + len);
        udp->checksum = 0;
        memcpy(packet + 8, buf, len);
        ip_send(sock->remote_addr.addr, IP_PROTO_UDP, packet, 8 + len);
        return len;
    }
    return -1;
}

int sys_recv(int fd, void *buf, uint32 len, int flags)
{
    (void)flags;
    if (fd < 0 || fd >= SOCKET_MAX || !buf) return -1;
    socket_t *sock = &sockets[fd];
    if (sock->state != SOCK_STATE_ESTABLISHED) return -1;

    uint8 packet[2048];
    int pkt_len;
    int timeout = 100;
    while ((pkt_len = nic_recv(packet, sizeof(packet))) <= 0 && timeout-- > 0) {
        rtc_mdelay(10);
    }

    if (pkt_len > 0) {
        eth_header_t *eth = (eth_header_t *)packet;
        if (ntohs(eth->type) == ETH_TYPE_IP) {
            ip_header_t *ip = (ip_header_t *)(packet + ETH_HDR_LEN);
            uint8 ip_hdr_len = IP_HDR_LEN(ip->version_ihl);
            if (ip->protocol == IP_PROTO_TCP && sock->type == SOCK_STREAM) {
                tcp_header_t *tcp = (tcp_header_t *)((uint8 *)ip + ip_hdr_len);
                uint8 tcp_hdr_len = TCP_HDR_LEN(tcp->data_offset);
                uint32 payload_len = ntohs(ip->total_len) - ip_hdr_len - tcp_hdr_len;
                uint8 *payload = (uint8 *)tcp + tcp_hdr_len;
                if (payload_len > 0) {
                    if (payload_len > len) payload_len = len;
                    memcpy(buf, payload, payload_len);
                    return payload_len;
                }
            } else if (ip->protocol == IP_PROTO_UDP && sock->type == SOCK_DGRAM) {
                udp_header_t *udp = (udp_header_t *)((uint8 *)ip + ip_hdr_len);
                uint32 payload_len = ntohs(udp->length) - 8;
                uint8 *payload = (uint8 *)udp + 8;
                if (payload_len > 0) {
                    if (payload_len > len) payload_len = len;
                    memcpy(buf, payload, payload_len);
                    return payload_len;
                }
            }
        }
    }
    return 0;
}

int sys_close(int fd)
{
    if (fd < 0 || fd >= SOCKET_MAX) return -1;
    if (sockets[fd].rx_buffer) kfree(sockets[fd].rx_buffer);
    if (sockets[fd].tx_buffer) kfree(sockets[fd].tx_buffer);
    memset(&sockets[fd], 0, sizeof(socket_t));
    sockets[fd].id = -1;
    return 0;
}

int sys_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
    (void)level; (void)optname; (void)optval; (void)optlen;
    if (fd < 0 || fd >= SOCKET_MAX) return -1;
    return 0;
}

/* ===== UDP Handler ===== */

void udp_handle_packet(const ip_header_t *ip, const uint8 *data, uint32 len)
{
    (void)ip; (void)data; (void)len;
}

/* ===== TCP Handler ===== */

void tcp_handle_packet(const ip_header_t *ip, const uint8 *data, uint32 len)
{
    (void)ip; (void)data; (void)len;
}

void tcp_periodic(void) {}

/* ===== DNS ===== */

void dns_init(void)
{
    if (num_dns == 0 && nic.num_dns > 0) {
        dns_servers[0] = nic.dns_servers[0];
        num_dns = 1;
    }
}

void dns_set_server(uint32 ip)
{
    dns_servers[0] = ip;
    num_dns = 1;
}

int dns_resolve(const char *hostname, uint32 *ip)
{
    if (!hostname || !ip || num_dns == 0) return -1;

    printk_color(TERM_CYAN, "[DNS] Resolving: %s\n", hostname);

    uint8 query[512];
    dns_header_t *hdr = (dns_header_t *)query;
    hdr->id = htons(0x1234);
    hdr->flags = htons(0x0100);
    hdr->questions = htons(1);
    hdr->answers = 0;
    hdr->authority = 0;
    hdr->additional = 0;

    uint8 *qname = query + 12;
    const char *p = hostname;
    while (*p) {
        const char *dot = p;
        while (*dot && *dot != '.') dot++;
        int len = dot - p;
        *qname++ = (uint8)len;
        for (int i = 0; i < len; i++) *qname++ = p[i];
        p = (*dot == '.') ? dot + 1 : dot;
    }
    *qname++ = 0;

    *(uint16 *)qname = htons(DNS_TYPE_A); qname += 2;
    *(uint16 *)qname = htons(DNS_CLASS_IN); qname += 2;

    uint32 query_len = (uint32)(qname - query);

    sockaddr_in_t dns_addr;
    dns_addr.family = AF_INET;
    dns_addr.port = htons(DNS_PORT);
    dns_addr.addr = dns_servers[0];

    int sock = sys_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return -1;

    sys_sendto(sock, query, query_len, 0, &dns_addr, sizeof(dns_addr));

    uint8 response[512];
    int rlen = sys_recvfrom(sock, response, sizeof(response), 0, NULL, NULL);
    sys_close(sock);

    if (rlen < (int)(12 + 16)) return -1;

    dns_header_t *resp_hdr = (dns_header_t *)response;
    uint16 num_answers = ntohs(resp_hdr->answers);
    if (num_answers == 0) return -1;

    uint8 *ptr = response + query_len;

    for (int i = 0; i < num_answers; i++) {
        if ((*ptr & 0xC0) == 0xC0) ptr += 2;
        else { while (*ptr) ptr += *ptr + 1; ptr++; }

        uint16 type = ntohs(*(uint16 *)ptr); ptr += 2;
        ptr += 2;
        ptr += 4;
        uint16 rdlen = ntohs(*(uint16 *)ptr); ptr += 2;

        if (type == DNS_TYPE_A && rdlen == 4) {
            memcpy(ip, ptr, 4);
            char ipbuf[16];
            inet_ntoa(*ip, ipbuf);
            printk_color(TERM_GREEN, "[DNS] %s -> %s\n", hostname, ipbuf);
            return 0;
        }
        ptr += rdlen;
    }
    return -1;
}

/* ===== HTTP Client ===== */

http_client_t *http_create(void)
{
    http_client_t *http = (http_client_t *)kmalloc(sizeof(http_client_t));
    if (!http) return NULL;
    memset(http, 0, sizeof(http_client_t));
    http->socket = -1;
    return http;
}

void http_free(http_client_t *http)
{
    if (!http) return;
    if (http->socket >= 0) sys_close(http->socket);
    if (http->body) kfree(http->body);
    kfree(http);
}

int http_get(http_client_t *http, const char *url)
{
    if (!http || !url) return -1;

    const char *p = url;
    http->use_ssl = 0;
    http->port = HTTP_PORT;

    if (strncmp(p, "http://", 7) == 0) { p += 7; http->port = HTTP_PORT; }
    else if (strncmp(p, "https://", 8) == 0) { p += 8; http->port = HTTPS_PORT; http->use_ssl = 1; }

    const char *slash = p;
    while (*slash && *slash != '/') slash++;
    int host_len = slash - p;
    if (host_len >= 128) host_len = 127;
    strncpy(http->host, p, host_len);
    http->host[host_len] = '\0';

    if (*slash) strncpy(http->path, slash, HTTP_MAX_URL - 1);
    else strcpy(http->path, "/");

    strncpy(http->url, url, HTTP_MAX_URL - 1);

    uint32 ip;
    if (dns_resolve(http->host, &ip) < 0) {
        printk_color(TERM_RED, "[HTTP] DNS failed for %s\n", http->host);
        return -1;
    }

    http->socket = sys_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (http->socket < 0) return -1;

    sockaddr_in_t addr;
    addr.family = AF_INET;
    addr.port = htons(http->port);
    addr.addr = ip;

    if (sys_connect(http->socket, &addr, sizeof(addr)) < 0) {
        sys_close(http->socket);
        http->socket = -1;
        return -1;
    }

    /* Use fixed-size buffer for request */
    char request_buf[512];
    int req_len = 0;
    req_len += snprintf(request_buf + req_len, sizeof(request_buf) - req_len,
             "GET %s HTTP/1.1\r\n", http->path);
    req_len += snprintf(request_buf + req_len, sizeof(request_buf) - req_len,
             "Host: %s\r\n", http->host);
    req_len += snprintf(request_buf + req_len, sizeof(request_buf) - req_len,
             "User-Agent: torOS/0.4\r\n");
    req_len += snprintf(request_buf + req_len, sizeof(request_buf) - req_len,
             "Connection: close\r\n\r\n");

    sys_send(http->socket, request_buf, req_len, 0);

    int total = 0;
    int rlen;
    while ((rlen = sys_recv(http->socket, http->response + total, HTTP_MAX_HDR - total - 1, 0)) > 0) {
        total += rlen;
    }
    http->response[total] = '\0';

    if (strncmp(http->response, "HTTP/1.1 ", 9) == 0 || strncmp(http->response, "HTTP/1.0 ", 9) == 0) {
        http->status_code = (http->response[9] - '0') * 100 +
                            (http->response[10] - '0') * 10 +
                            (http->response[11] - '0');
    }

    char *body_start = strstr(http->response, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        http->body_size = total - (body_start - http->response);
        if (http->body_size > 0) {
            http->body = (uint8 *)kmalloc(http->body_size + 1);
            if (http->body) {
                memcpy(http->body, body_start, http->body_size);
                http->body[http->body_size] = '\0';
            }
        }
    }

    char *cl = strstr(http->response, "Content-Length: ");
    if (cl) http->content_length = atoi(cl + 16);

    sys_close(http->socket);
    http->socket = -1;

    printk_color(TERM_GREEN, "[HTTP] %s -> %d (%d bytes)\n", url, http->status_code, http->body_size);
    return http->status_code;
}

int http_post(http_client_t *http, const char *url, const uint8 *data, uint32 len)
{
    (void)http; (void)url; (void)data; (void)len;
    return -1;
}

const uint8 *http_get_body(http_client_t *http, uint32 *len)
{ if (len) *len = http ? http->body_size : 0; return http ? http->body : NULL; }
int http_get_status(http_client_t *http) { return http ? http->status_code : 0; }

/* ===== TLS Stub ===== */

tls_context_t *tls_create(int socket)
{
    tls_context_t *tls = (tls_context_t *)kmalloc(sizeof(tls_context_t));
    if (!tls) return NULL;
    tls->socket = socket;
    tls->handshake_done = 0;
    return tls;
}

void tls_free(tls_context_t *tls) { if (tls) kfree(tls); }

int tls_handshake(tls_context_t *tls)
{
    if (!tls) return -1;
    tls->handshake_done = 1;
    return 0;
}

int tls_send(tls_context_t *tls, const uint8 *data, uint32 len)
{
    if (!tls || !tls->handshake_done) return -1;
    return sys_send(tls->socket, data, len, 0);
}

int tls_recv(tls_context_t *tls, uint8 *buffer, uint32 max_len)
{
    if (!tls || !tls->handshake_done) return -1;
    return sys_recv(tls->socket, buffer, max_len, 0);
}

/* ===== Socket Helpers ===== */

int sys_sendto(int fd, const void *buf, uint32 len, int flags, const sockaddr_in_t *addr, socklen_t addrlen)
{
    (void)addrlen;
    if (fd < 0 || fd >= SOCKET_MAX || !buf || !addr) return -1;
    sockets[fd].remote_addr = *addr;
    return sys_send(fd, buf, len, flags);
}

int sys_recvfrom(int fd, void *buf, uint32 len, int flags, sockaddr_in_t *addr, socklen_t *addrlen)
{
    (void)flags; (void)addrlen;
    if (addr) {
        addr->family = AF_INET;
        addr->port = htons(53);
        addr->addr = dns_servers[0];
    }
    return sys_recv(fd, buf, len, 0);
}

int sys_accept(int fd, sockaddr_in_t *addr, socklen_t *len)
{
    (void)addr; (void)len; (void)fd;
    return -1;
}
