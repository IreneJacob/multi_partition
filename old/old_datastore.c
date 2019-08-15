#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#include "datastore.h"

// initialize by setting the read, write options of a transaction database
void init_rocksdb_txdb(void) {
  uint64_t mem_budget = 1048576;
  txrocks.options = rocksdb_options_create();
  long cpus = sysconf(_SC_NPROCESSORS_ONLN);
  rocksdb_options_increase_parallelism(txrocks.options, (int)(cpus));
  rocksdb_options_optimize_level_style_compaction(txrocks.options, mem_budget);
  rocksdb_options_set_create_if_missing(txrocks.options, 1);

  txrocks.txdb_options = rocksdb_transactiondb_options_create();
  txrocks.tx_options = rocksdb_transaction_options_create();
  txrocks.write_options = rocksdb_writeoptions_create();
  rocksdb_writeoptions_disable_WAL(txrocks.write_options, 1);
  rocksdb_writeoptions_set_sync(txrocks.write_options, 0);
  txrocks.read_options = rocksdb_readoptions_create();
}

// to deconstruct, we delete the database options, transaction and close the
// database
int txdb_deconstruct(void) {
  if (txrocks.txdb_options) {
    rocksdb_transactiondb_options_destroy(txrocks.txdb_options);
  }
  if (txrocks.tx != NULL) {
    rocksdb_transaction_destroy(txrocks.tx);
  }
  if (txrocks.tx_db != NULL)
    rocksdb_transactiondb_close(txrocks.tx_db);
  return 0;
  txrocks.options == NULL;
  txrocks.tx = NULL;
  txrocks.tx_db = NULL;
}

// open a new database with the name provided in the argument (db represented as
// a folder)
int open_txdb(char *db_path) {
  char *err = NULL;
  txrocks.tx_db = rocksdb_transactiondb_open(
      txrocks.options, txrocks.txdb_options, db_path, &err);
  if (err != NULL) {
    printf("Cannot open DB: %s\n", err);
    return -1;
  }
  return 0;
}

// close (but not delete) the database
int close_txdb(void) {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return 0;
  }
  rocksdb_transactiondb_close(txrocks.tx_db);
  txrocks.tx_db = NULL;
  return 0;
}

// delete the database (removes folder)
int destroy_txdb(char *db_path) {
  char *err = NULL;
  rocksdb_destroy_db(txrocks.options, db_path, &err);
  if (err != NULL) {
    printf("Cannot destroy DB: %s\n", err);
    return -1;
  }
  return 0;
}

// create a new transaction in the current database
int begin_tx(void) {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  txrocks.tx = rocksdb_transaction_begin(txrocks.tx_db, txrocks.write_options,
                                         txrocks.tx_options, NULL);
  return 0;
}

// commit the transaction (changes will be saved to database)
int commit_tx(void) {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  char *err = NULL;
  rocksdb_transaction_commit(txrocks.tx, &err);
  if (err != NULL) {
    printf("%s\n", "Error committing transaction");
    return -1;
  }
  return 0;
}

// delete the transaction (changes that were not committed will not be saved to
// the database)
int delete_tx(void) {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  if (txrocks.tx != NULL) {
    rocksdb_transaction_destroy(txrocks.tx);
  }
  txrocks.tx = NULL;
  return 0;
}

// read values from the transaction (uncommitted changes will be read)
int handle_tx_read(char *key, size_t keylen) {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  char *err = NULL;
  char *val;
  size_t vallen;
  val = rocksdb_transaction_get(txrocks.tx, txrocks.read_options, key, keylen,
                                &vallen, &err);
  if (err != NULL) {
    fprintf(stdout, "Read Error: %s\n", err);
    return -1;
  }
  if (val == NULL) {
    printf("Value was not found for key %s\n", key);
    return 0;
  }
  printf("Found value %s, vallen %lu for key %s\n", val, vallen, key);
  free(val);
  return 0;
}

// write key, value pairs in the transaction
int handle_tx_write(char *key, size_t keylen, char *val, size_t vallen) {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  char *err = NULL;
  rocksdb_transaction_put(txrocks.tx, key, keylen, val, vallen, &err);
  if (err != NULL) {
    printf("%s %s, %s\n", "Error writing key value pair", key, val);
    return -1;
  }
  return 0;
}

// delete a value from the transaction (could also be used to delete a value
// from the database, if the tx is committed)
int handle_tx_delete(char *key, size_t keylen) {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  char *err = NULL;
  rocksdb_transaction_delete(txrocks.tx, key, keylen, &err);
  if (err != NULL) {
    printf("%s %s\n", "Error deleting entry", key);
    return -1;
  }
  return 0;
}

// create a save point in the transaction
int handle_save() {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  if (txrocks.tx == NULL) {
    fprintf(stderr, "No transaction created\n");
    return -1;
  }
  rocksdb_transaction_set_savepoint(txrocks.tx);
  return 0;
}

// rollback to saved point, changes after the savepoint will no longer be seen
// in the transaction
int handle_rollback() {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  if (txrocks.tx == NULL) {
    fprintf(stderr, "No transaction created\n");
    return -1;
  }
  char *err = NULL;
  rocksdb_transaction_rollback_to_savepoint(txrocks.tx, &err);
  if (err != NULL) {
    printf("%s\n", "Error in rolling back to savepoint");
    return -1;
  }
  return 0;
}

// list the key value pairs in the database, as if the transaction was committed
int handle_tx_list(void) {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  if (txrocks.tx == NULL) {
    fprintf(stderr, "no such transaction\n");
    return -1;
  }
  rocksdb_iterator_t *iterator =
      rocksdb_transaction_create_iterator(txrocks.tx, txrocks.read_options);
  if (!iterator) {
    fprintf(stderr, "Create Iterator has an error\n");
    return -1;
  }
  printf("%-20s | %-20s\n", "Key", "Value");
  printf("------------------------------------------------\n");
  for (rocksdb_iter_seek_to_first(iterator); rocksdb_iter_valid(iterator);
       rocksdb_iter_next(iterator)) {
    size_t klen;
    const char *key = rocksdb_iter_key(iterator, &klen);
    size_t vlen;
    const char *value = rocksdb_iter_value(iterator, &vlen);

    char ikey[klen + 1];
    strncpy(ikey, key, klen);
    ikey[klen] = '\0';
    char ivalue[vlen + 1];
    strncpy(ivalue, value, vlen);
    ivalue[vlen] = '\0';
    printf("%-20s | %-20s\n", ikey, ivalue);
  }
  rocksdb_iter_destroy(iterator);
  return 0;
}

// list the key value pairs in the database without considering the changes made
// by an uncommitted transaction
int handle_txdb_list(void) {
  if (txrocks.tx_db == NULL) {
    fprintf(stderr, "DB is not opened\n");
    return -1;
  }
  rocksdb_iterator_t *iterator = rocksdb_transactiondb_create_iterator(
      txrocks.tx_db, txrocks.read_options);
  if (!iterator) {
    fprintf(stderr, "Create Iterator has an error\n");
    return -1;
  }
  printf("%-20s | %-20s\n", "Key", "Value");
  printf("------------------------------------------------\n");
  for (rocksdb_iter_seek_to_first(iterator); rocksdb_iter_valid(iterator);
       rocksdb_iter_next(iterator)) {
    size_t klen;
    const char *key = rocksdb_iter_key(iterator, &klen);
    size_t vlen;
    const char *value = rocksdb_iter_value(iterator, &vlen);

    char ikey[klen + 1];
    strncpy(ikey, key, klen);
    ikey[klen] = '\0';
    char ivalue[vlen + 1];
    strncpy(ivalue, value, vlen);
    ivalue[vlen] = '\0';
    printf("%-20s | %-20s\n", ikey, ivalue);
  }
  rocksdb_iter_destroy(iterator);
  return 0;
}
