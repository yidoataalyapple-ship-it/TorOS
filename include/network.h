/*
 * torOS Network Stack Header
 * NIC, Ethernet, ARP, ICMP, TCP/IP, Socket API, DNS, HTTP, TLS
 */

#ifndef _NETWORK_H
#define _NETWORK_H

#include "toros.h"

/* ===== Ethernet ===== */
#define ETH_HDR_LEN         14
#define ETH_MTU             1500
#define ETH_ADDR_LEN        6
#define ETH_TYPE_IP         0x0800
#define ETH_TYPE_ARP        0x0806
#define ETH_TYPE_IPV6       0x86DD

typedef struct {
    uint8 dst[ETH_ADDR_LEN];
    uint8 src[ETH_ADDR_LEN];
    uint16 type;
} __attribute__((packed)) eth_header_t;

/* ===== ARP ===== */
#define ARP_HTYPE_ETH       1
#define ARP_PTYPE_IP        0x0800
#define ARP_OP_REQUEST      1
#define ARP_OP_REPLY        2

#define ARP_CACHE_SIZE      32

typedef struct {
    uint16 htype;
    uint16 ptype;
    uint8 hlen;
    uint8 plen;
    uint16 opcode;
    uint8 sender_mac[ETH_ADDR_LEN];
    uint32 sender_ip;
    uint8 target_mac[ETH_ADDR_LEN];
    uint32 target_ip;
} __attribute__((packed)) arp_packet_t;

typedef struct {
    uint32 ip;
    uint8 mac[ETH_ADDR_LEN];
    uint64 timestamp;
    uint8 valid;
} arp_cache_entry_t;

/* ===== IP ===== */
#define IP_PROTO_ICMP       1
#define IP_PROTO_TCP        6
#define IP_PROTO_UDP        17

typedef struct {
    uint8 version_ihl;
    uint8 tos;
    uint16 total_len;
    uint16 id;
    uint16 flags_frag;
    uint8 ttl;
    uint8 protocol;
    uint16 checksum;
    uint32 src_ip;
    uint32 dst_ip;
} __attribute__((packed)) ip_header_t;

#define IP_HDR_LEN(ihl)     (((ihl) & 0x0F) * 4)

/* ===== ICMP ===== */
#define ICMP_ECHO_REPLY     0
#define ICMP_ECHO_REQUEST   8

typedef struct {
    uint8 type;
    uint8 code;
    uint16 checksum;
    uint16 id;
    uint16 seq;
} __attribute__((packed)) icmp_header_t;

/* ===== UDP ===== */
typedef struct {
    uint16 src_port;
    uint16 dst_port;
    uint16 length;
    uint16 checksum;
} __attribute__((packed)) udp_header_t;

/* ===== TCP ===== */
typedef struct {
    uint16 src_port;
    uint16 dst_port;
    uint32 seq_num;
    uint32 ack_num;
    uint8 data_offset;
    uint8 flags;
    uint16 window;
    uint16 checksum;
    uint16 urgent;
} __attribute__((packed)) tcp_header_t;

/* TCP flags */
#define TCP_FIN             0x01
#define TCP_SYN             0x02
#define TCP_RST             0x04
#define TCP_PSH             0x08
#define TCP_ACK             0x10
#define TCP_URG             0x20

#define TCP_HDR_LEN(off)    ((((off) >> 4) & 0x0F) * 4)

/* ===== Socket API ===== */
#define AF_INET             2
#define SOCK_STREAM         1
#define SOCK_DGRAM          2
#define SOCK_RAW            3

#define IPPROTO_TCP         6
#define IPPROTO_UDP         17

#define SOL_SOCKET          1
#define SO_REUSEADDR        2
#define SO_BROADCAST        6

typedef uint32 socklen_t;

typedef struct {
    uint16 family;
    uint16 port;
    uint32 addr;
    uint8 pad[8];
} __attribute__((packed)) sockaddr_in_t;

typedef struct {
    uint32 s_addr;
} in_addr_t;

#define INADDR_ANY          0x00000000
#define INADDR_BROADCAST    0xFFFFFFFF
#define INADDR_LOOPBACK     0x7F000001

#define SOCKET_MAX          32

typedef enum {
    SOCK_STATE_UNUSED,
    SOCK_STATE_CLOSED,
    SOCK_STATE_LISTENING,
    SOCK_STATE_SYN_SENT,
    SOCK_STATE_SYN_RECEIVED,
    SOCK_STATE_ESTABLISHED,
    SOCK_STATE_FIN_WAIT_1,
    SOCK_STATE_FIN_WAIT_2,
    SOCK_STATE_CLOSE_WAIT,
    SOCK_STATE_CLOSING,
    SOCK_STATE_LAST_ACK,
    SOCK_STATE_TIME_WAIT,
    SOCK_STATE_BOUND
} socket_state_t;

typedef struct {
    int id;
    int domain;
    int type;
    int protocol;
    socket_state_t state;
    sockaddr_in_t local_addr;
    sockaddr_in_t remote_addr;
    uint32 seq_num;
    uint32 ack_num;
    uint16 window_size;
    uint8 *rx_buffer;
    uint32 rx_size;
    uint32 rx_head;
    uint32 rx_tail;
    uint8 *tx_buffer;
    uint32 tx_size;
    uint32 tx_head;
    uint32 tx_tail;
    uint64 timeout;
    int nonblocking;
} socket_t;

/* ===== TCP Connection State Machine ===== */
#define TCP_MAX_CONNECTIONS 16

typedef struct {
    uint32 local_ip;
    uint16 local_port;
    uint32 remote_ip;
    uint16 remote_port;
    socket_state_t state;
    uint32 seq_num;
    uint32 ack_num;
    uint32 snd_una;
    uint32 snd_nxt;
    uint32 rcv_nxt;
    uint16 window;
    uint16 mss;
    uint8 active;
} tcp_connection_t;

/* ===== DNS ===== */
#define DNS_PORT            53
#define DNS_MAX_NAME        256
#define DNS_MAX_SERVERS     3

typedef struct {
    uint16 id;
    uint16 flags;
    uint16 questions;
    uint16 answers;
    uint16 authority;
    uint16 additional;
} __attribute__((packed)) dns_header_t;

#define DNS_TYPE_A          1
#define DNS_TYPE_NS         2
#define DNS_TYPE_CNAME      5
#define DNS_TYPE_PTR        12
#define DNS_TYPE_MX         15

#define DNS_CLASS_IN        1

/* ===== HTTP ===== */
#define HTTP_PORT           80
#define HTTPS_PORT          443
#define HTTP_MAX_URL        512
#define HTTP_MAX_HDR        4096
#define HTTP_MAX_BODY       (1024 * 1024)

typedef struct {
    int socket;
    char url[HTTP_MAX_URL];
    char host[128];
    char path[HTTP_MAX_URL];
    uint16 port;
    int use_ssl;
    char request[HTTP_MAX_HDR];
    char response[HTTP_MAX_HDR];
    uint8 *body;
    uint32 body_size;
    int status_code;
    uint32 content_length;
    int chunked;
} http_client_t;

/* ===== TLS/SSL (simplified) ===== */
#define TLS_VERSION_1_2     0x0303
#define TLS_HANDSHAKE       22
#define TLS_CHANGE_CIPHER   20
#define TLS_ALERT           21
#define TLS_APPLICATION_DATA 23

#define TLS_CLIENT_HELLO    1
#define TLS_SERVER_HELLO    2
#define TLS_CERTIFICATE     11
#define TLS_SERVER_KEY_EX   12
#define TLS_CERTIFICATE_REQ 13
#define TLS_SERVER_HELLO_DONE 14
#define TLS_CLIENT_KEY_EX   16

/* ===== NIC ===== */
typedef struct {
    uint8 mac[ETH_ADDR_LEN];
    uint32 ip_addr;
    uint32 netmask;
    uint32 gateway;
    uint32 dns_servers[DNS_MAX_SERVERS];
    int num_dns;
    uint32 rx_packets;
    uint32 tx_packets;
    uint32 rx_bytes;
    uint32 tx_bytes;
    uint32 dropped;
    uint32 errors;
    void *mmio_base;
    int irq;
    int initialized;
    spinlock_t lock;
} nic_device_t;

/* ===== Network API ===== */
void net_init(void);
void net_shutdown(void);
int net_send_packet(const uint8 *data, uint32 len);
int net_recv_packet(uint8 *buffer, uint32 max_len);

/* NIC */
void nic_init(void);
int nic_send(const uint8 *data, uint32 len);
int nic_recv(uint8 *buffer, uint32 max_len);
const uint8 *nic_get_mac(void);
void nic_get_stats(uint32 *rx_pkts, uint32 *tx_pkts);

/* Ethernet */
void eth_send(const uint8 *dst_mac, uint16 type, const uint8 *data, uint32 len);
int eth_recv(uint8 *buffer, uint32 max_len, uint16 *type);
void eth_set_mac(const uint8 *mac);

/* ARP */
void arp_init(void);
void arp_send_request(uint32 target_ip);
void arp_send_reply(uint32 target_ip, const uint8 *target_mac);
int arp_resolve(uint32 ip, uint8 *mac);
void arp_handle_packet(const uint8 *data, uint32 len);
void arp_cache_dump(void);

/* IP */
void ip_init(void);
void ip_send(uint32 dst_ip, uint8 protocol, const uint8 *data, uint16 len);
int ip_recv(uint8 *buffer, uint32 max_len, uint8 *protocol);
uint16 ip_checksum(const void *data, uint32 len);

/* ICMP */
void icmp_send_echo(uint32 dst_ip, uint16 id, uint16 seq);
void icmp_handle_packet(const ip_header_t *ip, const uint8 *data, uint32 len);
int ping(uint32 dst_ip, uint16 count);

/* TCP */
void tcp_init(void);
socket_t *tcp_create_socket(void);
void tcp_close_socket(socket_t *sock);
int tcp_connect(socket_t *sock, uint32 dst_ip, uint16 dst_port);
int tcp_listen(socket_t *sock, uint16 port);
socket_t *tcp_accept(socket_t *sock);
int tcp_send(socket_t *sock, const uint8 *data, uint32 len);
int tcp_recv(socket_t *sock, uint8 *buffer, uint32 max_len);
void tcp_handle_packet(const ip_header_t *ip, const uint8 *data, uint32 len);
void tcp_periodic(void);

/* UDP */
void udp_init(void);
socket_t *udp_create_socket(void);
int udp_bind(socket_t *sock, uint16 port);
int udp_sendto(socket_t *sock, uint32 dst_ip, uint16 dst_port, const uint8 *data, uint32 len);
int udp_recvfrom(socket_t *sock, uint32 *src_ip, uint16 *src_port, uint8 *buffer, uint32 max_len);
void udp_handle_packet(const ip_header_t *ip, const uint8 *data, uint32 len);

/* Socket API */
void socket_init(void);
int sys_socket(int domain, int type, int protocol);
int sys_bind(int fd, const sockaddr_in_t *addr, socklen_t len);
int sys_listen(int fd, int backlog);
int sys_accept(int fd, sockaddr_in_t *addr, socklen_t *len);
int sys_connect(int fd, const sockaddr_in_t *addr, socklen_t len);
int sys_send(int fd, const void *buf, uint32 len, int flags);
int sys_recv(int fd, void *buf, uint32 len, int flags);
int sys_sendto(int fd, const void *buf, uint32 len, int flags, const sockaddr_in_t *addr, socklen_t addrlen);
int sys_recvfrom(int fd, void *buf, uint32 len, int flags, sockaddr_in_t *addr, socklen_t *addrlen);
int sys_close(int fd);
int sys_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen);

/* DNS */
void dns_init(void);
int dns_resolve(const char *hostname, uint32 *ip);
void dns_set_server(uint32 ip);

/* HTTP */
http_client_t *http_create(void);
void http_free(http_client_t *http);
int http_get(http_client_t *http, const char *url);
int http_post(http_client_t *http, const char *url, const uint8 *data, uint32 len);
const uint8 *http_get_body(http_client_t *http, uint32 *len);
int http_get_status(http_client_t *http);

/* TLS */
typedef struct { int socket; int handshake_done; uint8 master_secret[48]; } tls_context_t;
tls_context_t *tls_create(int socket);
void tls_free(tls_context_t *tls);
int tls_handshake(tls_context_t *tls);
int tls_send(tls_context_t *tls, const uint8 *data, uint32 len);
int tls_recv(tls_context_t *tls, uint8 *buffer, uint32 max_len);

/* Utility */
uint32 inet_addr(const char *ip_str);
void inet_ntoa(uint32 ip, char *buffer);
uint16 htons(uint16 v);
uint16 ntohs(uint16 v);
uint32 htonl(uint32 v);
uint32 ntohl(uint32 v);

#endif
