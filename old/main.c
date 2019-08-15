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

struct datastore_env_params rocksdb_env;

// TODO: function to give as param to deliver callback

int handle_db_message(struct app_lcore_params_worker *lp,
                       struct rte_mbuf *pkt_in) {
  size_t ip_offset = sizeof(struct ether_hdr);
  size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
  size_t app_offset = udp_offset + sizeof(struct udp_hdr);
  struct app_hdr *ap =
      rte_pktmbuf_mtod_offset(pkt_in, struct app_hdr *, app_offset);
  //  use a while loop to go through the commands, tokenize, and call the
  //  correct functions
  // first check if buffer is empty, if so, respond with no commands provided
  uint16_t msgtype = rte_be_to_cpu_16(ap->msgtype);
  uint16_t leader = rte_be_to_cpu_16(ap->leader);
  // uint8_t shard_mask = ap->shards; // the bitmap of partitions set
  // uint16_t num_shards =  __builtin_popcount (shard_mask); // total number of
  // shards
  int ret;
  int tx_failed = 0;
  int savepoint_count = 0;
  char message[MAXBUFSIZE];
  strcpy(message, (char *)ap->message);
  char *msg = strtok(message, "@");
  char val[MAXBUFSIZE];
  if (msgtype == 0) { // single partition
    // which shard is responsible
    if (leader == lp->worker_id) { // REVIEW: does this happen here or should
                                   // only the right lp have this message called
      // read the commands from the buffer - create the transaction
      ret = begin_transaction(&rocksdb_env,
                              rocksdb_env.workers[(int)leader].worker_id);
      if (ret >= 0) {
        ret = set_savepoint(&rocksdb_env,
                            rocksdb_env.workers[(int)leader].worker_id);
        if (ret >= 0) {
          savepoint_count++;
          while (msg) {
            printf("%s\n", msg);
            if (strncmp(msg, "r", 1) == 0) {
              strcpy(val, msg + 1);
              ret = handle_read(&rocksdb_env, val, rocksdb_env.workers[(int)leader].worker_id);
            } else if (strncmp(msg, "w", 1) == 0) {
              strcpy(val, msg + 1);
              ret = handle_write(&rocksdb_env, val, rocksdb_env.workers[(int)leader].worker_id);
            } else if (strncmp(msg, "d", 1) == 0) {
              strcpy(val, msg + 1);
              ret = handle_delete(&rocksdb_env, val, rocksdb_env.workers[(int)leader].worker_id);
            } else if (strncmp(msg, "s", 1) == 0) {
              ret = set_savepoint(&rocksdb_env,
                                  rocksdb_env.workers[(int)leader].worker_id);
              if (ret >= 0) {
                savepoint_count++;
              }
            } else if (strncmp(msg, "b", 1) == 0) {
              ret = rollback_to_last_savepoint(
                  &rocksdb_env,
                  rocksdb_env.workers[(int)leader].worker_id);
              if (ret >= 0) {
                savepoint_count--;
              }
            }
            msg = strtok(NULL, "@");
            if (ret < 0) {
              tx_failed = 1;
              break;
            }
          }
        } else {
          ret = delete_transaction(&rocksdb_env,
                                   rocksdb_env.workers[(int)leader].worker_id);
        }
      }
      if (tx_failed) {
        while (savepoint_count > 0) {
          ret = rollback_to_last_savepoint(
              &rocksdb_env,
              rocksdb_env.workers[(int)leader].worker_id);
          if (ret >= 0) {
            savepoint_count--;
          }
        }
        ret = delete_transaction(&rocksdb_env,
                                 rocksdb_env.workers[(int)leader].worker_id);
        return -1;
      } else {
        ret = commit_transaction(&rocksdb_env,
                                 rocksdb_env.workers[(int)leader].worker_id);
        savepoint_count = 0;
        return 0;
      }
    }
  } else if (msgtype == 1) { // multipartition
    // lock all other shards for the duration of the execution of this message
    for (int i = 0; i < rocksdb_env.num_workers; i++) {
      if (i != (int)leader) {
        pthread_mutex_lock(&rocksdb_env.workers[i].transaction_lock);
        while (!rocksdb_env.workers[i].transaction_done)
          pthread_cond_wait(&rocksdb_env.workers[i].transaction_cond,
                            &rocksdb_env.workers[i].transaction_lock);

        pthread_mutex_unlock(&rocksdb_env.workers[i].transaction_lock);
        // RTE_LOG(DEBUG, P4XOS, "Worker %u skipped inst %u\n", worker_id, inst);
        return -1;
      }
    }

    rocksdb_env.workers[leader].transaction_done = 0;
    // which shard is responsible
    if (leader == lp->worker_id) { // REVIEW: does this happen here or should
                                   // only the right lp have this message called
      // read the commands from the buffer - create the transaction
      while (msg) {
        printf("%s\n", msg);
        if (strncmp(msg, "r", 1) == 0) {
          strcpy(val, msg + 1);
          ret = handle_multidb_read(
              &rocksdb_env, val);
        } else if (strncmp(msg, "w", 1) == 0) {
          strcpy(val, msg + 1);
          ret = handle_multidb_write(
              &rocksdb_env, val);
        } else if (strncmp(msg, "d", 1) == 0) {
          strcpy(val, msg + 1);
          ret = handle_multidb_delete(
              &rocksdb_env, val);

        } else if (strncmp(msg, "s", 1) == 0) {
          ret = handle_multidb_setsavepoint(&rocksdb_env);
        } else if (strncmp(msg, "b", 1) == 0) {
          ret = handle_multidb_rollback(&rocksdb_env);
        }
        msg = strtok(NULL, "@");
        if (ret < 0) {
          tx_failed = 1;
          break;
        }
      }
      if (tx_failed) {
        int tx_to_delete;
        for (int i = 0; i < rocksdb_env.num_workers; i++) {
          tx_to_delete = 0;
          if (rocksdb_env.savepoint_count[i] > 0) {
            tx_to_delete = 1;
          }
          while (rocksdb_env.savepoint_count[i] > 0) {
            ret = rollback_to_last_savepoint(&rocksdb_env,
                                             i);
            if (ret >= 0) {
              rocksdb_env.savepoint_count[i]--;
            }
          }
          if (tx_to_delete) {
            ret = delete_transaction(&rocksdb_env, i);
          }
          if (i == (int)leader) {
            continue;
          }
          rocksdb_env.workers[i].transaction_done = 1; // REVIEW: set transaction done field to TRUE
          pthread_cond_signal(
              &rocksdb_env.workers[i].transaction_cond);
        }
        pthread_mutex_unlock(&rocksdb_env.workers[(int)leader].transaction_lock);
        return -1;
      } else {
        for (int i = 0; i < rocksdb_env.num_workers; i++) {
          if (rocksdb_env.savepoint_count[i] > 0) {
            ret = commit_transaction(&rocksdb_env, i);
            rocksdb_env.savepoint_count[i] = 0;
          }
          if (i == (int)leader) {
            continue;
          }
          rocksdb_env.workers[i].transaction_done = 1; // REVIEW: set transaction done field to TRUE
          pthread_cond_signal(
              &rocksdb_env.workers[i].transaction_cond);
        }
        pthread_mutex_unlock(&rocksdb_env.workers[(int)leader].transaction_lock);
        return 0;
      }
    }
  }
  return -1;
}

static void int_handler(int sig_num) {
  for (int i = 0; i < rocksdb_env.num_workers; i++) {
      char db_path[100];
      strcpy(db_path, rocksdb_env.db_path[i]);
      close_database(&rocksdb_env, i);
      destroy_database(rocksdb_env.options, db_path);
  }
  deconstruct_database_env(&rocksdb_env);
  printf("Exiting on signal %d\n", sig_num);
  /* set quit flag for thread to exit */
  app.force_quit = 1;
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
  // OPEN databases
  init_database_env(
      &rocksdb_env); // adds options to rocksdb stuff in app_params

  // TODO: call deliver callback
  /* Launch per-lcore init on every lcore */

  rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
  RTE_LCORE_FOREACH_SLAVE(lcore_id) {
    if (rte_eal_wait_lcore(lcore_id) < 0) {
      return -1;
    }
  }

  return 0;
}
