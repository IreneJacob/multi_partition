/**
 * @file datastore.h
 * @author Irene Jacob
 * @brief defines functions and structs used for the transactional database created with RocksDB
 * @version 0.1
 * @date 2019-06-24
 *
 * @copyright Copyright (c) 2019
 *
 */

#include "rocksdb/c.h"
#include "main.h"

#define MAX_NB_PARTITION 32
#define MAX_WORKER_CORE 4
#ifndef MAXBUFSIZE
#define MAXBUFSIZE 1024
#endif

enum app_message_type
{
	SINGLE = 0,
	MULTI = 1
};

typedef enum app_message_type app_message_type;
struct app_hdr{
  uint16_t msgtype;
	uint16_t shard; // leader shard
  char message[MAXBUFSIZE];
};

/**
 * @brief information stored by each worker thread
 *  REVIEW: work on the name and the description
 */
struct datastore_worker_params // now in struct app_lcore_params_worker
{
    char *db_path;
    rocksdb_transactiondb_t *tx_db;
    rocksdb_transaction_t *tx;
};

/**
 * @brief options for a rocksdb database
 * FIXME: the name of the struct might not be the most easy to understand
 */
struct datastore_env_params
{
    rocksdb_options_t *options;
    rocksdb_transactiondb_options_t *txdb_options;
    rocksdb_writeoptions_t *write_options;
    rocksdb_readoptions_t *read_options;
    rocksdb_transaction_options_t *tx_options;
    struct datastore_worker_params workers[MAX_WORKER_CORE]; // the other struct is defined first so that we dont get the incomplete elment type error here
    // char hostname[32]; //REVIEW: what's the use of this variable
};

/* Forward Declarations */
void init_database_env(struct datastore_env_params * rocksdb_env);
void deconstruct_database_env(struct datastore_env_params * rocksdb_env);
int open_database(struct datastore_worker_params * worker, char *name, rocksdb_options_t *db_options, rocksdb_transactiondb_options_t *txdb_options);
int close_database(struct datastore_worker_params * worker);
int destroy_database(rocksdb_options_t *options, char *db_path); //deletes the folder
int begin_transaction(struct datastore_worker_params * worker, rocksdb_writeoptions_t *write_options, rocksdb_transaction_options_t *tx_options);
int commit_transaction(struct datastore_worker_params * worker);
int delete_transaction(struct datastore_worker_params * worker);
int split_transaction(); // split a multi-partition tx to single partition transaction
int write_keyval_pair(struct datastore_worker_params * worker, char *key, size_t keylen, char *val, size_t vallen);
int read_keyval_pair(struct datastore_worker_params * worker, rocksdb_readoptions_t *read_options, char *key, size_t keylen);
int delete_keyval_pair(struct datastore_worker_params * worker, char *key, size_t keylen);
int set_savepoint(struct datastore_worker_params *worker);
int rollback_to_last_savepoint(struct datastore_worker_params *worker);

void handle_db_message(struct app_lcore_params_worker *lp, struct rte_mbuf *pkt_in);

/**
 * REVIEW: I'm not sure the following block of functions are really required
 */
void print_database_usage();
int parse_config();
void populate_config();
void free_config();
void print_parameters();

int handle_read(struct datastore_worker_params * worker, rocksdb_readoptions_t *read_options, char *key);
int handle_write(struct datastore_worker_params * worker, char *key);
int handle_delete(struct datastore_worker_params * worker, char *key);
// int handle_list();
// int handle_save();
// int handle_rollback();
