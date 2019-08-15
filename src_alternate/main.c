#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/queue.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_ether.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_lpm.h>
#include <rte_udp.h>

#include "datastore.h"
#include "main.h"

struct datastore_env_params rocksdb_env;
//
// static int init_database(__attribute__((unused)) void *arg)
// {
//     unsigned lcore_id;
//     lcore_id = rte_lcore_id();
//     printf("hello from core %u\n", lcore_id);
//     char db_path[20];
//     int ret;
//     snprintf(db_path, sizeof(db_path), "%u", lcore_id);
//     ret = open_database(db_path, rocksdb_env.options, rocksdb_env.txdb_options, &(rocksdb_env.workers[lcore_id]));
//     if (lcore_id == 2)
//     {
//
//         ret = begin_transaction(&(rocksdb_env.workers[lcore_id]), rocksdb_env.write_options, rocksdb_env.tx_options);
//         if (ret >= 0)
//         {
//             ret = set_savepoint(&(rocksdb_env.workers[lcore_id]));
//             ret = write_keyval_pair(&(rocksdb_env.workers[lcore_id]), "a", sizeof("a"), "2", sizeof("2"));
//             ret = set_savepoint(&(rocksdb_env.workers[lcore_id]));
//             ret = write_keyval_pair(&(rocksdb_env.workers[lcore_id]), "a", sizeof("a"), "4", sizeof("4"));
//             ret = read_keyval_pair(&(rocksdb_env.workers[lcore_id]),rocksdb_env.read_options, "a", sizeof("a"));
//             ret = rollback_to_last_savepoint(&(rocksdb_env.workers[lcore_id]));
//             ret = read_keyval_pair(&(rocksdb_env.workers[lcore_id]),rocksdb_env.read_options, "a", sizeof("a"));
//
//             // char *err = NULL;
//             // rocksdb_transaction_put(rocksdb_env.workers[lcore_id].tx, "a", sizeof("a"), "2", sizeof("2"), &err);
//             // if (err != NULL)
//             // {
//             //     printf("%s\n", "Error writing key value pair");
//             // }
//             ret = commit_transaction(&(rocksdb_env.workers[lcore_id]));
//         }
//     }
//     else
//     {
//         sleep(2);
//     }
//
//     close_database(&(rocksdb_env.workers[lcore_id]));
//     // destroy_database(rocksdb_env.options,db_path);
//     return 0;
// }

// TODO: function to give as param to deliver callback


void handle_db_message(struct app_lcore_params_worker *lp, struct rte_mbuf *pkt_in){
		size_t ip_offset = sizeof(struct ether_hdr);
		size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
		size_t app_offset = udp_offset + sizeof(struct udp_hdr);
		struct app_hdr *ap = rte_pktmbuf_mtod_offset(pkt_in, struct app_hdr *, app_offset);
    // TODO: handle the commands here
    //  use a while loop to go through the commands, stripwhite, and call the correct functions
    // first check if buffer is empty, if so, respond with no commands provided
    uint16_t msgtype = rte_be_to_cpu_16(ap->msgtype);
    uint16_t leader = rte_be_to_cpu_16(ap->leader);
    uint8_t shard_mask = ap->shards; // the bitmap of partitions set
    uint16_t num_shards =  __builtin_popcount (shard_mask); // total number of shards
    int ret;
    // int i;
    int abort = 0;
    int savepoint_count = 0;
    char message[MAXBUFSIZE];
    strcpy(message, (char * )ap->message);
    char * msg = strtok(message, "@");
    char val[MAXBUFSIZE];
    if (msgtype == 0) { // single partition
      // which shard is responsible
      if (leader == lp->worker_id) { // does this happen here or should only the right lp have this message called
        // read the commands from the buffer - create the transaction
        ret = begin_transaction(&rocksdb_env.workers[(int)leader], rocksdb_env.write_options, rocksdb_env.tx_options, rocksdb_env.workers[(int) leader].worker_id);
        if (ret >= 0) {
          ret = set_savepoint(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
          if (ret >= 0) {
            savepoint_count++;
            while (msg) {
              printf("%s\n", msg);
            if(strncmp(msg,"r",1)==0){
                strcpy(val,msg+1);
                ret = handle_read(&rocksdb_env.workers[(int)leader], rocksdb_env.read_options, val);
            }
            else if(strncmp(msg,"w",1)==0){
                strcpy(val,msg+1);
                ret = handle_write(&rocksdb_env.workers[(int)leader], val);
            }
            else if(strncmp(msg,"d",1)==0){
                strcpy(val,msg+1);
                ret = handle_delete(&rocksdb_env.workers[(int)leader], val);
            }
            else if(strncmp(msg,"s",1)==0){
                ret = set_savepoint(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
                if (ret >= 0) {
                  savepoint_count++;
                }
            }
            else if(strncmp(msg,"b",1)==0){
                ret = rollback_to_last_savepoint(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
                if (ret >= 0) {
                  savepoint_count--;
                }
            }
            // else if(strncmp(msg,"c",1)==0){
            //     ret = commit_transaction(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
            //     savepoint_count = 0;
            //     // deliver the transaction
            // }
            msg = strtok(NULL, "@");
            if (ret < 0) {
              abort = 1;
              break;
            }
          }
        }
        else{
          ret = delete_transaction(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
        }
      }
      if (abort) {
        while (savepoint_count > 0) {
          ret = rollback_to_last_savepoint(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
          if (ret >= 0) {
            savepoint_count--;
          }
        }
        ret = delete_transaction(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
      }
      else{
        ret = commit_transaction(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
        //     savepoint_count = 0;
      }
    }
  }
  else if(msgtype==1){ // multipartition
    // which shard is responsible
    if (leader == lp->worker_id) { // does this happen here or should only the right lp have this message called
      // read the commands from the buffer - create the transaction
      // ret = begin_transaction(&rocksdb_env.workers[(int)leader], rocksdb_env.write_options, rocksdb_env.tx_options, rocksdb_env.workers[(int) leader].worker_id);
      // if (ret >= 0) {
      //   ret = set_savepoint(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
        // if (ret >= 0) {
        //   savepoint_count++;
          while (msg) {
            printf("%s\n", msg);
          if(strncmp(msg,"r",1)==0){
              strcpy(val,msg+1);
              ret = handle_multidb_read(&rocksdb_env.workers[(int)leader], rocksdb_env.read_options, val, rocksdb_env.num_workers);
          }
          else if(strncmp(msg,"w",1)==0){
              strcpy(val,msg+1);
              ret = handle_multidb_write(&rocksdb_env.workers[(int)leader], val, rocksdb_env.num_workers);
          }
          else if(strncmp(msg,"d",1)==0){
              strcpy(val,msg+1);
              ret = handle_multidb_delete(&rocksdb_env.workers[(int)leader], val);
          }
          else if(strncmp(msg,"s",1)==0){
              ret = handle_multidb_setsavepoint(&rocksdb_env.workers[(int)leader], rocksdb_env.num_workers);
          }
          else if(strncmp(msg,"b",1)==0){
              ret = handle_multidb_rollback(&rocksdb_env.workers[(int)leader], rocksdb_env.num_workers);
          }
          // else if(strncmp(msg,"c",1)==0){
          //     ret = handle_multidb_commit(&rocksdb_env.workers[(int)leader], rocksdb_env.workers[(int) leader].worker_id);
          //     // deliver the transaction
          // }
          msg = strtok(NULL, "@");
          if (ret < 0) {
            abort = 1;
            break;
          }
        }
      // }
    // }
      if (abort) {
        int tx_to_delete;
        for (int i = 0; i < rocksdb_env.num_workers; i++) {
          tx_to_delete = 0;
          if (rocksdb_env.workers[(int)leader].savepoint_count_multidb[i] > 0) {
            tx_to_delete = 1;
          }
          while (rocksdb_env.workers[(int)leader].savepoint_count_multidb[i] > 0) {
            ret = rollback_to_last_savepoint(&rocksdb_env.workers[(int)leader], i);
            if (ret >= 0) {
              rocksdb_env.workers[(int)leader].savepoint_count_multidb[i]--;
            }
          }
          if (tx_to_delete) {
            ret = delete_transaction(&rocksdb_env.workers[(int)leader], i);
          }
        }
      }
      else{
        for (int i = 0; i < rocksdb_env.num_workers; i++) {
          if (rocksdb_env.workers[(int)leader].savepoint_count_multidb[i] > 0) {
            ret = commit_transaction(&rocksdb_env.workers[(int)leader], i);
            rocksdb_env.workers[(int)leader].savepoint_count_multidb[i] = 0;
          }
        }
      }
    }
  }
}


static void
int_handler(int sig_num)
{

  deconstruct_database_env(&rocksdb_env);
  for (int i = 0; i < APP_MAX_LCORES; i++)
  {
      if (app.lcore_params[i].type == e_APP_LCORE_WORKER)
      {
          char db_path[100];
          strcpy(db_path,rocksdb_env.workers[i].db_path[i]);
          close_database(&rocksdb_env.workers[i], rocksdb_env.num_workers, i);
          destroy_database(rocksdb_env.options,db_path);
      }
  }
  printf("Exiting on signal %d\n", sig_num);
	/* set quit flag for thread to exit */
	app.force_quit = 1;
}

int main(int argc, char *argv[])
{
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
    signal(SIGUSR1, int_handler);

    /* Parse application arguments (after the EAL ones) */
    ret = app_parse_args(argc, argv);
    if (ret < 0)
    {
        app_print_usage();
        return -1;
    }
    /* Init */
    app_init();
    app_print_params();
    // OPEN databases
    init_database_env(&rocksdb_env); // adds options to rocksdb stuff in app_params

    // if (ret < 0)
    // {
    //     rte_panic("Cannot init EAL\n");
    // }
    // init_database_env();
    // RTE_LCORE_FOREACH_SLAVE(lcore_id)
    // {
    //     rte_eal_remote_launch(init_database, NULL, lcore_id);
    // }

    // TODO: call deliver callback
    /* Launch per-lcore init on every lcore */

    rte_eal_mp_remote_launch(app_lcore_main_loop, NULL, CALL_MASTER);
    RTE_LCORE_FOREACH_SLAVE(lcore_id)
    {
        if (rte_eal_wait_lcore(lcore_id) < 0)
        {
            return -1;
        }
    }
    // init_database(NULL); // master thread does the same functionality
    // deconstruct_database_env(&rocksdb_env);
    // for (i = 0; i < APP_MAX_LCORES; i++)
    // {
    //     if (app.lcore_params[i].type == e_APP_LCORE_WORKER)
    //     {
    //         char db_path[100];
    //         strcpy(db_path,rocksdb_env.workers[i].db_path);
    //         close_database(&rocksdb_env.workers[i]);
    //         destroy_database(rocksdb_env.options,db_path);
    //     }
    // }

    return 0;
}
