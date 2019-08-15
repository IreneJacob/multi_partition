#include "rocksdb/c.h"
#define MAX_WORKER_CORE 4

// TODO: Check whether the parameters are still applicable for the transactional rocksdb settings - if not, make necessary changes

struct rocksdb_lcore_params
{
    /* single database - managed by a worker thread*/
    char *db_path;
    rocksdb_transactiondb_t *tx_db; // the database
    rocksdb_checkpoint_t *cp;
    rocksdb_backup_engine_t *be; // REVIEW: what is this backup engine?
    uint32_t delivered_count;
    uint32_t write_count;
    uint32_t read_count;
    pthread_mutex_t transaction_lock;
    pthread_cond_t transaction_cond;
    uint32_t transaction_done;
};

struct rocksdb_txdb_params
{
    rocksdb_options_t *options;                    // options for the base database (not transaction database)
    rocksdb_transactiondb_options_t *txdb_options; // options for the transaction database
    rocksdb_writeoptions_t *write_options;         // write options
    rocksdb_readoptions_t *read_options;           // read options
    rocksdb_transaction_t *tx;                     // transaction pointer
    rocksdb_transaction_options_t *tx_options;     // transaction options
    struct rocksdb_lcore_params worker[MAX_WORKER_CORE];
    uint32_t num_workers; // REVIEW: why uint32_t, what is this var used for?
    uint32_t total_delivered_count; // what is this used for?
    char hostname[32];
};

// REVIEW: The following functions may not all be used/ may be changed

extern struct rocksdb_configurations rocksdb_configurations;

void rocksdb_print_usage(void);
int parse_rocksdb_configuration(int argc, char **argv);
void populate_configuration(char *config_file, struct rocksdb_configurations *conf);
void free_rocksdb_configurations(struct rocksdb_configurations *conf);
void print_parameters(void);
int init_rocksdb(struct rocksdb_params *lp);
void handle_put(struct rocksdb_t *db, struct rocksdb_writeoptions_t *writeoptions,
                const char *key, uint32_t keylen,
                const char *value, uint32_t vallen);
char *handle_get(struct rocksdb_t *db, struct rocksdb_readoptions_t *readoptions,
                 const char *key, uint32_t keylen, size_t *vallen);
void handle_checkpoint(struct rocksdb_checkpoint_t *cp, const char *cp_path);
void handle_backup(struct rocksdb_t *db, rocksdb_backup_engine_t *be);
void display_rocksdb_statistics(struct rocksdb_params *lp);
void lcore_cleanup(struct rocksdb_lcore_params *lp);
void cleanup(struct rocksdb_params *lp);

void init_rocksdb_txdb(void);
int txdb_deconstruct(void);
int open_txdb(char *db_path);
int close_txdb(void);
int destroy_txdb(char *db_path);
int begin_tx(void);
int commit_tx(void);
int delete_tx(void);
int handle_tx_read(char *key, size_t keylen);
int handle_tx_write(char *key, size_t keylen, char *val, size_t vallen);
int handle_tx_delete(char *key, size_t keylen);
int handle_tx_list(void);
int handle_txdb_list(void);
int handle_save(void);
int handle_rollback(void);

void t_init_rocksdb_txdb(struct rocksdb_txdb_params txrocksdb);
int t_txdb_deconstruct(struct rocksdb_txdb_params txrocksdb);
int t_open_txdb(struct rocksdb_txdb_params txrocksdb, char* db_path);
int t_close_txdb(struct rocksdb_txdb_params txrocksdb);
int t_destroy_txdb(struct rocksdb_txdb_params txrocksdb, char* db_path);
int t_begin_tx(struct rocksdb_txdb_params txrocksdb);
int t_commit_tx(struct rocksdb_txdb_params txrocksdb);
int t_delete_tx(struct rocksdb_txdb_params txrocksdb);
int t_handle_tx_read(struct rocksdb_txdb_params txrocksdb, char *key, size_t keylen);
int t_handle_tx_write(struct rocksdb_txdb_params txrocksdb, char *key, size_t keylen, char *val, size_t vallen);
int t_handle_tx_delete(struct rocksdb_txdb_params txrocksdb, char *key, size_t keylen);
int t_handle_tx_list(struct rocksdb_txdb_params txrocksdb);
int t_handle_txdb_list(struct rocksdb_txdb_params txrocksdb);
int t_handle_save(struct rocksdb_txdb_params txrocksdb);
int t_handle_rollback(struct rocksdb_txdb_params txrocksdb);
extern struct rocksdb_txdb_params txrocks;
