#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_interrupts.h>
#include <rte_ip.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_lpm.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>
#include <rte_memory.h>
#include <rte_mempool.h>
#include <rte_per_lcore.h>
#include <rte_prefetch.h>
#include <rte_random.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#include "datastore.h"
#include "main.h"
#include "net_util.h"

// struct datastore_env_params rocksdb_env;
void prepare_hw_checksum(struct rte_mbuf *pkt_in, size_t data_size);


// TODO: function to give as param to deliver callback
int handle_propose(struct rte_mbuf *pkt_in, void *arg) {

  struct app_lcore_params_worker *lp = (struct app_lcore_params_worker *)arg;
  size_t ip_offset = sizeof(struct ether_hdr);
  struct ipv4_hdr *ip =
  rte_pktmbuf_mtod_offset(pkt_in, struct ipv4_hdr *, ip_offset);
  size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
  size_t app_offset = udp_offset + sizeof(struct udp_hdr);
  struct app_hdr *ap =
      rte_pktmbuf_mtod_offset(pkt_in, struct app_hdr *, app_offset);
  uint16_t msgtype = rte_be_to_cpu_16(ap->msgtype);
  int ret;
  if (msgtype == 0) { // packet was successfully received by server
    // TODO: print key values of any reads
    // set ip addresses
    ip->dst_addr = app.server.sin_addr.s_addr;
    ip->src_addr = app.client.sin_addr.s_addr;
    // set_ip_addr(ip, app.p4xos_conf.mine.sin_addr.s_addr,
    //             app.p4xos_conf.paxos_leader.sin_addr.s_addr);

    size_t data_size = sizeof(struct app_hdr);
    prepare_hw_checksum(pkt_in, data_size);
    return 0;
  }
  return -1;
}

int app_get_lcore_worker(uint32_t worker_id) {
  int lcore;

  for (lcore = 0; lcore < APP_MAX_LCORES; lcore++) {
    if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
      continue;
    }
    struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;
    if (lp->worker_id == worker_id)
      return lcore;
  }

  return -1;
}

void submit_bulk(uint8_t worker_id, uint32_t nb_pkts,
    struct app_lcore_params_worker *lp, char *value, int size) {
    int ret;
    uint32_t mbuf_idx;
    uint16_t port = 0;

    int lcore = app_get_lcore_worker(worker_id);
    if (lcore < 0) {
        rte_panic("Invalid worker_id\n");
    }

    struct rte_mbuf *pkts[nb_pkts];
    ret = rte_pktmbuf_alloc_bulk(app.lcore_params[lcore].pool, pkts, nb_pkts);

    if (ret < 0) {
        RTE_LOG(INFO, USER1, "Not enough entries in the mempools\n");
        return;
    }

    uint32_t i;
    for (i = 0; i < nb_pkts; i++) {
      printf("%s\n", "reaches prepare paxos message");
        prepare_paxos_message(pkts[i], port, &app.client,
                        &app.server,
                        0, 0, worker_id, value, size);

        mbuf_idx = lp->mbuf_out[port].n_mbufs;
        lp->mbuf_out[port].array[mbuf_idx++] = pkts[i];
        lp->mbuf_out[port].n_mbufs = mbuf_idx;
    }
    lp->mbuf_out_flush[port] = 1;
}


static void int_handler(int sig_num) {
  printf("Exiting on signal %d\n", sig_num);
  /* set quit flag for thread to exit */
  app.force_quit = 1;
}

static int parse_arg_ip_address(const char *arg, struct sockaddr_in *addr) {
  int ret;
  char *ip_and_port = strdup(arg);
  const char delim[2] = ":";
  char *token = strtok(ip_and_port, delim);
  addr->sin_family = AF_INET;
  if (token != NULL) {
    ret = inet_pton(AF_INET, token, &addr->sin_addr);
    if (ret == 0 || ret < 0) {
      return -1;
    }
  }
  token = strtok(NULL, delim);
  if (token != NULL) {
    uint32_t x;
    char *endpt;
    errno = 0;
    x = strtoul(token, &endpt, 10);
    if (errno != 0 || endpt == arg || *endpt != '\0') {
      return -2;
    }
    addr->sin_port = htons(x);
  }
  return 0;
}

int main(int argc, char *argv[]) {
  int ret;
  // int i;
  unsigned lcore_id;
  /* Init EAL */
  ret = rte_eal_init(argc, argv);

  if (ret < 0)
    return -1;
  argc -= ret;
  argv += ret;

  /* catch SIGUSR1 so we can print on exit */
  signal(SIGINT, int_handler);

  /* Parse application arguments (after the EAL ones) */
  ret = app_parse_args(argc, argv);
  if (ret < 0) {
    app_print_usage();
    return -1;
  }
  /* Init */
  app_init();
  app_print_params();

  //FIXME HARDCODED CLIENT AND SERVER ADDRESSES AND PORT
  const char client_addr_arg[200] = "192.168.4.95:27461";
  const char server_addr_arg[200] = "192.168.4.98:9081";

  ret = parse_arg_ip_address(server_addr_arg, &(app.server));
  if (ret) {
    printf("Incorrect value for --leader-addr argument (%d)\n", ret);
    return -1;
  }
  ret = parse_arg_ip_address(client_addr_arg, &(app.client));
  if (ret) {
    printf("Incorrect value for --src argument (%d)\n", ret);
    return -1;
  }


  // FIXME: SEND A Request
  uint32_t n_workers = app_get_lcores_worker();

  char val[MAXBUFSIZE] = "@w 1 a @w 2 b @r 1 @ w 3 c";
	uint32_t worker_id = 0;

	for (int lcore = 0; lcore < APP_MAX_LCORES; lcore ++) {
		struct app_lcore_params_worker *lp = &app.lcore_params[lcore].worker;

		if (app.lcore_params[lcore].type != e_APP_LCORE_WORKER) {
			continue;
		}
    printf("%s\n", "reaches submit bulk" );
		submit_bulk(worker_id, 1, lp, val, sizeof(val)); // sizeof in order to always be the same instead of strlen

		worker_id++;
		if (worker_id == n_workers)
			break;
	}


  // TODO: call deliver callback
  app_set_worker_callback(handle_propose);
  /* Launch per-lcore init on every lcore */

  rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
  RTE_LCORE_FOREACH_SLAVE(lcore_id) {
    if (rte_eal_wait_lcore(lcore_id) < 0) {
      return -1;
    }
  }

  return 0;
}
