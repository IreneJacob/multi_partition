/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
// needed for ping
#include <arpa/inet.h>
#include <rte_arp.h>
#include <rte_icmp.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_log.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

#define RTE_LOGTYPE_PAXOS RTE_LOGTYPE_USER1

#define ETHER_TYPE_STP 0x6900

uint32_t my_ip = IPv4(192, 168, 4, 96);

static const struct rte_eth_conf port_conf_default = {
    .rxmode = {
        .max_rx_pkt_len = ETHER_MAX_LEN,
    },
};

static struct
{
    uint64_t total_cycles;
    uint64_t total_pkts;
} latency_numbers;

static void prepare_hw_checksum(struct rte_mbuf *pkt_in, size_t data_size)
{
    size_t ip_offset = sizeof(struct ether_hdr);
    struct ipv4_hdr *ip =
        rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
    ip->hdr_checksum = 0;
    size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
    struct udp_hdr *udp =
        rte_pktmbuf_mtod_offset(pkt_in, struct udp_hdr *, udp_offset);
    udp->dgram_len = rte_cpu_to_be_16(sizeof(struct udp_hdr) + data_size);
    pkt_in->l2_len = sizeof(struct ether_hdr);
    pkt_in->l3_len = sizeof(struct ipv4_hdr);
    pkt_in->l4_len = sizeof(struct udp_hdr) + data_size;
    size_t pkt_size = pkt_in->l2_len + pkt_in->l3_len + pkt_in->l4_len;
    pkt_in->data_len = pkt_size;
    pkt_in->pkt_len = pkt_size;
    pkt_in->ol_flags = PKT_TX_IPV4 | PKT_TX_IP_CKSUM | PKT_TX_UDP_CKSUM;
    udp->dgram_cksum = rte_ipv4_phdr_cksum(ip, pkt_in->ol_flags);
}

static int
process_pkt(uint16_t port, struct rte_mbuf *pkt)
{
    struct arp_hdr *arp_hdr;
    char src[INET_ADDRSTRLEN];
    char dst[INET_ADDRSTRLEN];
    // uint32_t ip_dst;
    int ret = -1;
    struct ipv4_hdr *ipv4_hdr;
    struct udp_hdr *udp_hdr;
    struct icmp_hdr *icmp_hdr;

    struct ether_addr d_addr;

    uint32_t bond_ip = rte_cpu_to_be_32(my_ip);

    struct ether_hdr *eth_hdr = rte_pktmbuf_mtod_offset(pkt, struct ether_hdr *, 0);
    size_t ip_offset = sizeof(struct ether_hdr);

    switch (rte_be_to_cpu_16(eth_hdr->ether_type))
    {
    case ETHER_TYPE_ARP:
        arp_hdr = rte_pktmbuf_mtod_offset(pkt, struct arp_hdr *, ip_offset);
        inet_ntop(AF_INET, &(arp_hdr->arp_data.arp_sip), src, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(arp_hdr->arp_data.arp_tip), dst, INET_ADDRSTRLEN);
        RTE_LOG(DEBUG, PAXOS, "ARP: %s -> %s\n", src, dst);
        if (arp_hdr->arp_data.arp_tip == bond_ip)
        {
            if (arp_hdr->arp_op == rte_cpu_to_be_16(ARP_OP_REQUEST))
            {
                RTE_LOG(DEBUG, PAXOS, "ARP Request\n");
                arp_hdr->arp_op = rte_cpu_to_be_16(ARP_OP_REPLY);
                /* Switch src and dst data and set bonding MAC */
                ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
                rte_eth_macaddr_get(port, &eth_hdr->s_addr);
                ether_addr_copy(&arp_hdr->arp_data.arp_sha, &arp_hdr->arp_data.arp_tha);
                arp_hdr->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;
                rte_eth_macaddr_get(port, &d_addr);
                ether_addr_copy(&d_addr, &arp_hdr->arp_data.arp_sha);
                arp_hdr->arp_data.arp_sip = bond_ip;
                // ip_dst = rte_be_to_cpu_32(bond_ip);
                ret = 0;
            }
        }
        else
        {
            ret = -1;
        }
        break;
    case ETHER_TYPE_IPv4:
        ipv4_hdr = rte_pktmbuf_mtod_offset(pkt, struct ipv4_hdr *, ip_offset);
        size_t l4_offset = ip_offset + sizeof(struct ipv4_hdr);
        inet_ntop(AF_INET, &(ipv4_hdr->src_addr), src, INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(ipv4_hdr->dst_addr), dst, INET_ADDRSTRLEN);
        // ip_dst = rte_be_to_cpu_32(ipv4_hdr->dst_addr);

        RTE_LOG(DEBUG, PAXOS, "IPv4: %s -> %s Proto %u\n", src, dst, ipv4_hdr->next_proto_id);

        switch (ipv4_hdr->next_proto_id)
        {
        case IPPROTO_UDP:
            // TODO: is it the same as the ICMP case?
            udp_hdr = rte_pktmbuf_mtod_offset(pkt, struct udp_hdr *, l4_offset);
            RTE_LOG(DEBUG, PAXOS, "UDP: %s -> %s: Port: %02x\n", src, dst, udp_hdr->dst_port);
            ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
            rte_eth_macaddr_get(port, &eth_hdr->s_addr);
            ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
            ipv4_hdr->src_addr = bond_ip;
            uint16_t src_port = udp_hdr->src_port;
            udp_hdr->src_port = udp_hdr->dst_port;
            udp_hdr->dst_port = src_port;

            size_t payload_offset = l4_offset + sizeof(udp_hdr);
            size_t payload_size = rte_be_to_cpu_16(udp_hdr->dgram_len) - sizeof(struct udp_hdr);
            printf("Size of datagram %lu\n", payload_size);
            char *msg = rte_pktmbuf_mtod_offset(pkt, char *, payload_offset);
            strncpy(msg, "Echo From Server", payload_size - 1);
            msg[payload_size - 1] = '\0';
            prepare_hw_checksum(pkt, payload_size);
            // ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
            // rte_eth_macaddr_get(port, &eth_hdr->s_addr);
            // ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
            // udp_hdr->src_addr = bond_ip;
            ret = 0;
            break;
        case IPPROTO_ICMP:
            icmp_hdr = rte_pktmbuf_mtod_offset(pkt, struct icmp_hdr *, l4_offset);
            RTE_LOG(DEBUG, PAXOS, "ICMP: %s -> %s: Type: %02x\n", src, dst, icmp_hdr->icmp_type);
            if (icmp_hdr->icmp_type == IP_ICMP_ECHO_REQUEST)
            {
                if (ipv4_hdr->dst_addr == bond_ip)
                {
                    icmp_hdr->icmp_type = IP_ICMP_ECHO_REPLY;
                    ether_addr_copy(&eth_hdr->s_addr, &eth_hdr->d_addr);
                    rte_eth_macaddr_get(port, &eth_hdr->s_addr);
                    ipv4_hdr->dst_addr = ipv4_hdr->src_addr;
                    ipv4_hdr->src_addr = bond_ip;
                    // ip_dst = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
                    ret = 0;
                }
            }
            break;
        default:
            ret = -1;
            RTE_LOG(DEBUG, PAXOS, "IP Proto: %d\n", ipv4_hdr->next_proto_id);
            break;
        }
        break;
    case ETHER_TYPE_STP:
        RTE_LOG(DEBUG, PAXOS, "STP message\n");
        ret = -1;
        break;

    default:
        RTE_LOG(DEBUG, PAXOS, "Ether Proto: 0x%04x\n", rte_be_to_cpu_16(eth_hdr->ether_type));
        ret = -1;
        break;
    }
    return ret;
}

static uint16_t
add_timestamps(uint16_t port __rte_unused, uint16_t qidx __rte_unused,
               struct rte_mbuf **pkts, uint16_t nb_pkts,
               uint16_t max_pkts __rte_unused, void *_ __rte_unused)
{

    unsigned i;
    int ret;
    uint64_t now = rte_rdtsc();
    uint16_t nb_rx = nb_pkts;

    for (i = 0; i < nb_rx; i++)
    {
        pkts[i]->udata64 = now;
        ret = process_pkt(port, pkts[i]);
        if (ret < 0)
        {
            rte_pktmbuf_free(pkts[i]);
            nb_pkts--;
        }
    }

    return nb_pkts;
}

static uint16_t
calc_latency(uint16_t port __rte_unused, uint16_t qidx __rte_unused,
             struct rte_mbuf **pkts, uint16_t nb_pkts, void *_ __rte_unused)
{
    uint64_t cycles = 0;
    uint64_t now = rte_rdtsc();
    unsigned i;

    for (i = 0; i < nb_pkts; i++)
        cycles += now - pkts[i]->udata64;
    latency_numbers.total_cycles += cycles;
    latency_numbers.total_pkts += nb_pkts;

    if (latency_numbers.total_pkts > (100 * 1000 * 1000ULL))
    {
        printf("Latency = %" PRIu64 " cycles\n",
               latency_numbers.total_cycles / latency_numbers.total_pkts);
        latency_numbers.total_cycles = latency_numbers.total_pkts = 0;
    }
    return nb_pkts;
}

/*
 * Initialises a given port using global settings and with the rx buffers
 * coming from the mbuf_pool passed as parameter
 */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
    struct rte_eth_conf port_conf = port_conf_default;
    const uint16_t rx_rings = 1, tx_rings = 1;
    uint16_t nb_rxd = RX_RING_SIZE;
    uint16_t nb_txd = TX_RING_SIZE;
    int retval;
    uint16_t q;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    if (!rte_eth_dev_is_valid_port(port))
        return -1;

    rte_eth_dev_info_get(port, &dev_info);
    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
        port_conf.txmode.offloads |=
            DEV_TX_OFFLOAD_MBUF_FAST_FREE;

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM)
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_UDP_CKSUM)
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_UDP_CKSUM;

    if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_TCP_CKSUM)
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_TCP_CKSUM;

    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if (retval != 0)
        return retval;

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if (retval != 0)
        return retval;

    for (q = 0; q < rx_rings; q++)
    {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                                        rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if (retval < 0)
            return retval;
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;
    for (q = 0; q < tx_rings; q++)
    {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd,
                                        rte_eth_dev_socket_id(port), &txconf);
        if (retval < 0)
            return retval;
    }

    retval = rte_eth_dev_start(port);
    if (retval < 0)
        return retval;

    struct ether_addr addr;

    rte_eth_macaddr_get(port, &addr);
    printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
           " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
           (unsigned)port,
           addr.addr_bytes[0], addr.addr_bytes[1],
           addr.addr_bytes[2], addr.addr_bytes[3],
           addr.addr_bytes[4], addr.addr_bytes[5]);

    rte_eth_promiscuous_enable(port);
    rte_eth_add_rx_callback(port, 0, add_timestamps, NULL);
    rte_eth_add_tx_callback(port, 0, calc_latency, NULL);

    return 0;
}

/*
 * Main thread that does the work, reading from INPUT_PORT
 * and writing to OUTPUT_PORT
 */
static __attribute__((noreturn)) void
lcore_main(void)
{
    uint16_t port;

    RTE_ETH_FOREACH_DEV(port)
    if (rte_eth_dev_socket_id(port) > 0 &&
        rte_eth_dev_socket_id(port) !=
            (int)rte_socket_id())
        printf("WARNING, port %u is on remote NUMA node to "
               "polling thread.\n\tPerformance will "
               "not be optimal.\n",
               port);

    printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n",
           rte_lcore_id());
    for (;;)
    {
        RTE_ETH_FOREACH_DEV(port)
        {
            struct rte_mbuf *bufs[BURST_SIZE];
            const uint16_t nb_rx = rte_eth_rx_burst(port, 0,
                                                    bufs, BURST_SIZE);
            if (unlikely(nb_rx == 0))
                continue;

            const uint16_t nb_tx = rte_eth_tx_burst(port, 0,
                                                    bufs, nb_rx);
            if (unlikely(nb_tx < nb_rx))
            {
                uint16_t buf;

                for (buf = nb_tx; buf < nb_rx; buf++)
                    rte_pktmbuf_free(bufs[buf]);
            }
        }
    }
}

/* Main function, does initialisation and calls the per-lcore functions */
int main(int argc, char *argv[])
{
    struct rte_mempool *mbuf_pool;
    uint16_t nb_ports;
    uint16_t portid;

    /* init EAL */
    int ret = rte_eal_init(argc, argv);

    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    argc -= ret;
    argv += ret;

    rte_log_set_level(RTE_LOGTYPE_PAXOS, rte_log_get_global_level());

    nb_ports = rte_eth_dev_count_avail();

    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL",
                                        NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* initialize all ports */
    RTE_ETH_FOREACH_DEV(portid)
    if (port_init(portid, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %" PRIu8 "\n",
                 portid);

    if (rte_lcore_count() > 1)
        printf("\nWARNING: Too much enabled lcores - "
               "App uses only 1 lcore\n");

    /* call lcore_main on master core only */
    lcore_main();
    return 0;
}