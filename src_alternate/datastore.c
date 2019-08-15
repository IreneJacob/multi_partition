/**
 * @file datastore.c
 * @author Irene Jacob
 * @brief
 * @version 0.1
 * @date 03-07-2019
 *
 * @copyright Copyright (c) 2019
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "datastore.h"
#include "main.h"
#include <unistd.h>

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
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_lpm.h>
//
// #ifndef MAXKEYLEN
// #define MAXKEYLEN 20
// #endif
//
// #ifndef MAXVALLEN
// #define MAXVALLEN 20
// #endif

// typedef struct {
//     char *name;			/* User printable name of the function. */
//     rl_icpfunc_t *func;		/* Function to call to do the job. */
//     char *doc;			/* Documentation for this function.  */
// } COMMAND;
//
// COMMAND commands[] = {
//     { "open", com_open, "Open a transaction database" },
//     { "tx_create", com_tx_create, "Create a transaction" },
//     { "write", com_write, "Write a (key,value) pair" },
//     { "read", com_read, "Read the value associated with a key" },
//     { "delete", com_delete, "Delete a key" },
//     { "commit", com_tx_commit, "Commit the transaction" },
//     { "tx_destroy", com_tx_destroy, "Destroy the transaction" },
//     { "save", com_tx_set_savepoint, "Set a save point for the transaction" },
//     { "rollback", com_tx_rollback, "Rollback the transaction to previously saved point" },
//     { "tx_list", com_tx_list, "List all keys and values in the transaction" },
//     { "list", com_list, "List all keys and values in the database"},
//     { "help", com_help, "Display this text" },
//     { "?", com_help, "Synonym for `help'" },
//     { "close", com_close, "Close the opened database" },
//     { "destroy", com_db_destroy, "delete the database" },
//     { "quit", com_quit, "Quit using rocks" },
//     { "exit", com_quit, "Synonym for `quit'" },
//     { (char *)NULL, (rl_icpfunc_t *)NULL, (char *)NULL }
// };

int handle_read(struct datastore_worker_params * worker, rocksdb_readoptions_t *read_options, char *key){
  char * str = strtok(key, " ");
  int ret  = read_keyval_pair(worker, read_options, str, strlen(str));
  return ret;
}

int handle_delete(struct datastore_worker_params * worker, char *key){
  char * str = strtok(key, " ");
  int ret = delete_keyval_pair(worker, str, strlen(str));
  return ret;
}

int handle_write(struct datastore_worker_params * worker, char *key){
  char * k = strtok(key, " ");
  char * v = strtok(NULL, " ");
  int ret = write_keyval_pair(worker, k, strlen(k), v, strlen(v));
  return ret;
}
//
// void handle_db_message(struct datastore_worker_params *lp, struct rte_mbuf *pkt_in){
// 		size_t ip_offset = sizeof(struct ether_hdr);
// 		size_t udp_offset = ip_offset + sizeof(struct ipv4_hdr);
// 		size_t app_offset = udp_offset + sizeof(struct udp_hdr);
// 		struct app_hdr *ap = rte_pktmbuf_mtod_offset(pkt_in, struct app_hdr *, app_offset);
//     // TODO: handle the commands here
//     //  use a while loop to go through the commands, stripwhite, and call the correct functions
//     // first check if buffer is empty, if so, respond with no commands provided
//     uint16_t msgtype = rte_be_to_cpu_16(ap->msgtype);
//     uint16_t shard = rte_be_to_cpu_16(ap->shard);
//     int ret;
//     // int i;
//     int abort = 0;
//     int savepoint_count = 0;
//     char message[MAXBUFSIZE];
//     strcpy(message, (char * )ap->message);
//     char * msg = strtok(message, "@");
//     char val[MAXBUFSIZE];
//     if (msgtype == 0) { // single partition
//       // which shard is responsible
//       if (shard == lp->worker_id) { // does this happen here or should only the right lp have this message called
//         //REVIEW DATABASE SHOULD BE OPENED AT INIT
//         // read the commands from the buffer - create the transaction
//         ret = begin_transaction(lp, app.write_options, app.tx_options);
//         if (ret >= 0) {
//           ret = set_savepoint(lp);
//           if (ret >= 0) {
//             savepoint_count++;
//             while (msg) {
//               printf("%s\n", msg);
//             if(strncmp(msg,"r",1)==0){
//                 strcpy(val,msg+1);
//                 ret = handle_read(lp, rocksdb_env.read_options, val);
//             }
//             else if(strncmp(msg,"w",1)==0){
//                 strcpy(val,msg+1);
//                 ret = handle_write(lp, val);
//             }
//             else if(strncmp(msg,"d",1)==0){
//                 strcpy(val,msg+1);
//                 ret = handle_delete(lp, val);
//             }
//             else if(strncmp(msg,"s",1)==0){
//                 ret = set_savepoint(lp);
//                 if (ret >= 0) {
//                   savepoint_count++;
//                 }
//             }
//             else if(strncmp(msg,"b",1)==0){
//                 ret = rollback_to_last_savepoint(lp);
//                 if (ret >= 0) {
//                   savepoint_count--;
//                 }
//             }
//             else if(strncmp(msg,"c",1)==0){
//                 ret = commit_transaction(lp);
//                 // deliver the transaction
//             }
//             msg = strtok(NULL, "@");
//             if (ret < 0) {
//               abort = 1;
//               break;
//             }
//           }
//         }
//       }
//       if (abort) {
//         while (savepoint_count > 0) {
//           ret = rollback_to_last_savepoint(lp);
//         }
//       }
//     }
//   }

    // uint16_t msgtype = rte_be_to_cpu_16(ap->msgtype);
    // // int ret;
		// switch(msgtype)
		// {
		// 	case SINGLE: {
    //     // handle the type of request here for single db
		// 	}
		// 	case MULTI: {
    //     // TODO: handle multi_database requests here
		// 	}
		// 	default:
		// 		printf("No handler for %u\n", msgtype);
		// }
// }

/**
 * @brief
 *
 */
void init_database_env(struct datastore_env_params * rocksdb_env)
{
    rocksdb_env->options = rocksdb_options_create();
    rocksdb_env->txdb_options = rocksdb_transactiondb_options_create();
    rocksdb_env->write_options = rocksdb_writeoptions_create();
    rocksdb_env->read_options = rocksdb_readoptions_create();
    rocksdb_env->tx_options = rocksdb_transaction_options_create();
    rocksdb_options_set_create_if_missing(rocksdb_env->options, 1);

    uint32_t num_workers = app_get_lcores_worker();
    for (int i = 0; i < (int) num_workers; i++) {
      //open the databases
      char db_path[50];
      sprintf(db_path,"%d", i);
      int ret = open_database(&rocksdb_env->workers[i], db_path, rocksdb_env->options, rocksdb_env->txdb_options);
      if (ret < 0) {
        printf("Unable to open db\n");
      }
    }
}

/**
 * @brief extern ROCKSDB_LIBRARY_API rocksdb_transactiondb_t* rocksdb_transactiondb_open(
    const rocksdb_options_t* options,
    const rocksdb_transactiondb_options_t* txn_db_options, const char* name,
    char** errptr);
 *
 */
int open_database(struct datastore_worker_params *worker ,char *name, rocksdb_options_t *db_options, rocksdb_transactiondb_options_t *txdb_options)
{
    char *err = NULL;
    if (worker->db_path != NULL || worker->tx_db != NULL)
    {
        fprintf(stderr, "Database already assigned to worker\n");
        return -1;
    }
    worker->tx_db = rocksdb_transactiondb_open(db_options, txdb_options, (const char *)name, &err);
    if (err != NULL)
    {
        printf("An error occurred while creating the database %s \n", name);
        printf("%s\n", err);
        return -1;
    }
    worker->db_path = malloc(20);
    strcpy(worker->db_path, name);
    printf("Database %s opened successfully\n", worker->db_path);
    return 0;
}

/**
 * @brief extern ROCKSDB_LIBRARY_API void rocksdb_transactiondb_close(
    rocksdb_transactiondb_t* txn_db);
 *
 * @return int
 */
int close_database(struct datastore_worker_params *worker)
{
    if (worker->tx_db == NULL) // check on the actual database not the name
    {
        printf("Database not open\n");
        return 0; // db doesnt exist
    }
    if (worker->tx != NULL)
    {
        printf("Transaction will be aborted\n");
    }
    rocksdb_transactiondb_close(worker->tx_db);
    worker->tx_db = NULL;
    free(worker->db_path);
    worker->db_path = NULL;
    worker->tx = NULL;
    printf("Database closed successfully \n");
    return 0;
}

void deconstruct_database_env(struct datastore_env_params * rocksdb_env)
{
    if (rocksdb_env->txdb_options)
    {
        rocksdb_transactiondb_options_destroy(rocksdb_env->txdb_options);
        rocksdb_env->txdb_options = NULL;
    }
    if (rocksdb_env->options)
    {
        rocksdb_options_destroy(rocksdb_env->options);
        rocksdb_env->options = NULL;
    }
    if (rocksdb_env->write_options)
    {
        rocksdb_writeoptions_destroy(rocksdb_env->write_options);
        rocksdb_env->write_options = NULL;
    }
    if (rocksdb_env->read_options)
    {
        rocksdb_readoptions_destroy(rocksdb_env->read_options);
        rocksdb_env->read_options = NULL;
    }
    if (rocksdb_env->tx_options)
    {
        rocksdb_transaction_options_destroy(rocksdb_env->tx_options);
        rocksdb_env->tx_options = NULL;
    }
}

/**
 * @brief delete's the database and removes the folder
 * we have to provide the name, because when we close the database, we set worker.db_path to NULL and free the memory, so we have to make sure we are deleting the right database
 * @return int
 */
int destroy_database(rocksdb_options_t *options, char *db_path)
{
    char *err = NULL;
    rocksdb_destroy_db(options, db_path, &err);
    if (err != NULL)
    {
        printf("Cannot delete database\n");
        printf("%s\n", err);
        return -1;
    }

    return 0;
}

/**
 * @brief extern ROCKSDB_LIBRARY_API rocksdb_transaction_t* rocksdb_transaction_begin(
    rocksdb_transactiondb_t* txn_db,
    const rocksdb_writeoptions_t* write_options,
    const rocksdb_transaction_options_t* txn_options,
    rocksdb_transaction_t* old_txn);
 *
 * @param worker
 * @return int
 */
int begin_transaction(struct datastore_worker_params *worker, rocksdb_writeoptions_t *write_options, rocksdb_transaction_options_t *tx_options)
{
    if (worker->tx_db == NULL)
    {
        fprintf(stderr, "Database %s is not open for begin tx\n", worker->db_path);
        return -1;
    }
    if (worker->tx != NULL) // REVIEW: should we reuse the transaction?
    {
        // TODO: keep transactions in a FIFO queue
        fprintf(stderr, "Another transaction is in process\n");
        return -1;
    }
    // FIXME: function called returns a pointer to a transaction, we're storing it in the struct itself
    worker->tx = rocksdb_transaction_begin(worker->tx_db, write_options, tx_options, NULL);
    return 0;
}

/**
 * @brief extern ROCKSDB_LIBRARY_API void rocksdb_transaction_commit(
    rocksdb_transaction_t* txn, char** errptr);
 *
 * @param worker
 * @return int
 */
int commit_transaction(struct datastore_worker_params *worker)
{
    char *err = NULL;
    if (worker->tx_db == NULL)
    {
        printf("Database is not open for commit\n");
        return -1;
    }
    if (worker->tx == NULL)
    {
        printf("No transaction to commit\n");
        return -1;
    }
    rocksdb_transaction_commit(worker->tx, &err);
    if (err != NULL)
    {
        printf("An error occured while attempting to commit transaction \n");
        printf("%s\n", err);
        return -1;
    }
    printf("Transaction committed successfully\n");
    worker->tx = NULL; // REVIEW: should we reuse the transaction
    return 0;
}

int delete_transaction(struct datastore_worker_params *worker)
{
    if (worker->tx_db == NULL)
    {
        printf("Database is not open for delete\n");
        return -1;
    }
    if (worker->tx == NULL)
    {
        printf("No transaction to delete\n");
        return 0;
    }
    rocksdb_transaction_destroy(worker->tx);
    worker->tx = NULL;
    return 0;
}
int split_transaction() { return 0; }
int write_keyval_pair(struct datastore_worker_params *worker, char *key, size_t keylen, char *val, size_t vallen)
{
    if (worker->tx_db == NULL)
    {
        printf("Database is not open for write\n");
        return -1;
    }
    if (worker->tx == NULL)
    {
        printf("Begin a transaction first\n");
        return -1;
    }
    char *err = NULL;
    rocksdb_transaction_put(worker->tx, key, keylen, val, vallen, &err);
    if (err != NULL)
    {
        printf("%s %s, %s\n", "Error writing key value pair", key, val);
        printf("%s\n", err);
        return -1;
    }
    printf("Key: %s, Value: %s pair was written successfully\n", key, val);
    return 0;
}

int read_keyval_pair(struct datastore_worker_params *worker, rocksdb_readoptions_t *read_options, char *key, size_t keylen)
{
    if (worker->tx_db == NULL)
    {
        fprintf(stderr, "Database is not open for read\n");
        return -1;
    }
    if (worker->tx == NULL)
    {
        fprintf(stderr, "Begin a transaction first\n");
        return -1;
    }
    char *err = NULL;
    char *val = NULL;
    size_t vallen;
    val = rocksdb_transaction_get(worker->tx, read_options, key, keylen, &vallen, &err);
    if (err != NULL)
    {
        fprintf(stderr, "There was an error reading the value\n");
        printf("%s\n", err);
        return -1;
    }
    if (val == NULL)
    {
        fprintf(stderr, "Value was not found for key %s\n", key);
        return -1; // REVIEW: should this be return 0?
    }
    fprintf(stdout, "Value for key %s is %s\n", key, val);
    free(val);
    return 0;
}

/**
 * @brief delete a key from the database.
 * the transaction must be committed for the key to be deleted from the database.
 */
int delete_keyval_pair(struct datastore_worker_params * worker, char *key, size_t keylen)
{
    if (worker->tx_db == NULL)
    {
        printf("Database is not open\n");
        return -1;
    }
    if (worker->tx == NULL)
    {
        printf("Begin a transaction first\n");
        return -1;
    }

    char *err = NULL;
    rocksdb_transaction_delete(worker->tx, key, keylen, &err);
    if (err != NULL)
    {
        printf("There was an error deleting the entry\n");
        printf("%s\n", err);
        return -1;
    }
    return 0;
}

int set_savepoint(struct datastore_worker_params * worker)
{
    if (worker->tx_db == NULL)
    {
        printf("Database is not open\n");
        return -1;
    }
    if (worker->tx == NULL)
    {
        printf("Begin a transaction first\n");
        return -1;
    }
    rocksdb_transaction_set_savepoint(worker->tx);
    return 0;
}

int rollback_to_last_savepoint(struct datastore_worker_params  *worker)
{
    if (worker->tx_db == NULL)
    {
        printf("Database is not open\n");
        return -1;
    }
    if (worker->tx == NULL)
    {
        printf("Begin a transaction first\n");
        return -1;
    }
    char *err = NULL;
    rocksdb_transaction_rollback_to_savepoint(worker->tx, &err);
    if (err != NULL)
    {
        printf("Error in rolling back to savepoint\n");
        printf("%s\n", err);
        return -1;
    }
    return 0;
}
