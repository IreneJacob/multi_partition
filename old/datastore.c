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
#include "datastore.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#include <rte_ring.h>
#include <rte_tcp.h>
#include <rte_udp.h>

#ifndef MAX_NUM_KEYS
#define MAX_NUM_KEYS 4000
#endif

int handle_multidb_read(struct datastore_env_params *rocks, char *key) {
  int ret;
  char *str = strtok(key, " ");
  int k = atoi(str);
  int lower_bound = 0;
  for (int i = 0; i < rocks->num_workers; i++) {
    if (k >= lower_bound && k < (MAX_NUM_KEYS / rocks->num_workers)) {
      if (rocks->savepoint_count[i] == 0) {
        ret = begin_transaction(rocks, i);
        if (ret >= 0) {
          ret = set_savepoint(rocks, i);
          if (ret < 0) {
            ret = delete_transaction(rocks, i);
            return -1;
          } else {
            rocks->savepoint_count[i] = 1;
          }
        } else {
          return -1;
        }
      }
      ret = read_keyval_pair(rocks, str, strlen(str), i);
      return ret;
    } else {
      lower_bound += (MAX_NUM_KEYS / rocks->num_workers);
    }
  }
  return -1;
}

// int handle_multidb_read(struct datastore_worker_params *worker,
//                         rocksdb_readoptions_t *read_options,
//                         rocksdb_writeoptions_t *write_options,
//                         rocksdb_transaction_options_t *tx_options, char *key,
//                         int num_partitions) {
//   int ret;
//   char *str = strtok(key, " ");
//   int k = atoi(str);
//   int lower_bound = 0;
//   for (int i = 0; i < num_partitions; i++) {
//     if (k >= lower_bound && k < (MAX_NUM_KEYS / num_partitions)) {
//       if (worker->savepoint_count[i] == 0) {
//         ret = begin_transaction(worker, write_options, tx_options, i);
//         if (ret >= 0) {
//           ret = set_savepoint(worker, i);
//           if (ret < 0) {
//             ret = delete_transaction(worker, i);
//             return -1;
//           } else {
//             worker->savepoint_count[i] = 1;
//           }
//         } else {
//           return -1;
//         }
//       }
//       ret = read_keyval_pair(worker, read_options, str, strlen(str), i);
//       return ret;
//     } else {
//       lower_bound += (MAX_NUM_KEYS / num_partitions);
//     }
//   }
//   return -1;
// }

int handle_read(struct datastore_env_params *rocks, char *key, int db_id) {
  char *str = strtok(key, " ");
  // FIXME: what if there was a commit message before this - we should create a
  // transaction in this case
  int ret = read_keyval_pair(rocks, str, strlen(str), db_id);
  return ret;
}
// int handle_read(struct datastore_worker_params *worker,
//                 rocksdb_readoptions_t *read_options, char *key) {
//   char *str = strtok(key, " ");
//   // FIXME: what if there was a commit message before this - we should create
//   a
//   // transaction in this case
//   int ret = read_keyval_pair(worker, read_options, str, strlen(str),
//                              worker->worker_id);
//   return ret;
// }

int handle_multidb_delete(struct datastore_env_params *rocks, char *key) {
  int ret;
  char *str = strtok(key, " ");
  int k = atoi(str);
  int lower_bound = 0;
  for (int i = 0; i < rocks->num_workers; i++) {
    if (k >= lower_bound && k < (MAX_NUM_KEYS / rocks->num_workers)) {
      if (rocks->savepoint_count[i] == 0) {
        ret = begin_transaction(rocks, i);
        if (ret >= 0) {
          ret = set_savepoint(rocks, i);
          if (ret < 0) {
            ret = delete_transaction(rocks, i);
            return -1;
          } else {
            rocks->savepoint_count[i] = 1;
          }
        } else {
          return -1;
        }
      }
      ret = delete_keyval_pair(rocks, str, strlen(str), i);
      return ret;
    } else {
      lower_bound += (MAX_NUM_KEYS / rocks->num_workers);
    }
  }
  return -1;
}

// int handle_multidb_delete(struct datastore_worker_params *worker,
//                           rocksdb_writeoptions_t *write_options,
//                           rocksdb_transaction_options_t *tx_options, char *key,
//                           int num_partitions) {
//   int ret;
//   char *str = strtok(key, " ");
//   int k = atoi(str);
//   int lower_bound = 0;
//   for (int i = 0; i < num_partitions; i++) {
//     if (k >= lower_bound && k < (MAX_NUM_KEYS / num_partitions)) {
//       if (worker->savepoint_count[i] == 0) {
//         ret = begin_transaction(worker, write_options, tx_options, i);
//         if (ret >= 0) {
//           ret = set_savepoint(worker, i);
//           if (ret < 0) {
//             ret = delete_transaction(worker, i);
//             return -1;
//           } else {
//             worker->savepoint_count[i] = 1;
//           }
//         } else {
//           return -1;
//         }
//       }
//       ret = delete_keyval_pair(worker, str, strlen(str), i);
//       return ret;
//     } else {
//       lower_bound += (MAX_NUM_KEYS / num_partitions);
//     }
//   }
//   return -1;
// }

int handle_delete(struct datastore_env_params *rocks, char *key,
                  int db_id) {
  char *str = strtok(key, " ");
  int ret = delete_keyval_pair(rocks, str, strlen(str), db_id);
  return ret;
}

// int handle_delete(struct datastore_worker_params *worker, char *key) {
//   char *str = strtok(key, " ");
//   int ret = delete_keyval_pair(worker, str, strlen(str), worker->worker_id);
//   return ret;
// }

int handle_multidb_write(struct datastore_env_params *rocks, char *key) {
  int ret;
  char *k = strtok(key, " ");
  char *v = strtok(NULL, " ");
  int num_key = atoi(k);
  int lower_bound = 0;
  for (int i = 0; i < rocks->num_workers; i++) {
    if (num_key >= lower_bound && num_key < (MAX_NUM_KEYS / rocks->num_workers)) {
      if (rocks->savepoint_count[i] == 0) {
        ret = begin_transaction(rocks, i);
        if (ret >= 0) {
          ret = set_savepoint(rocks, i);
          if (ret < 0) {
            ret = delete_transaction(rocks, i);
            return -1;
          } else {
            rocks->savepoint_count[i] = 1;
          }
        } else {
          return -1;
        }
      }
      ret = write_keyval_pair(rocks, k, strlen(k), v, strlen(v), i);
      return ret;
    } else {
      lower_bound += (MAX_NUM_KEYS / rocks->num_workers);
    }
  }
  return -1;
}

// int handle_multidb_write(struct datastore_worker_params *worker,
//                          rocksdb_writeoptions_t *write_options,
//                          rocksdb_transaction_options_t *tx_options, char *key,
//                          int num_partitions) {
//   int ret;
//   char *k = strtok(key, " ");
//   char *v = strtok(NULL, " ");
//   int num_key = atoi(k);
//   int lower_bound = 0;
//   for (int i = 0; i < num_partitions; i++) {
//     if (num_key >= lower_bound && num_key < (MAX_NUM_KEYS / num_partitions)) {
//       if (worker->savepoint_count[i] == 0) {
//         ret = begin_transaction(worker, write_options, tx_options, i);
//         if (ret >= 0) {
//           ret = set_savepoint(worker, i);
//           if (ret < 0) {
//             ret = delete_transaction(worker, i);
//             return -1;
//           } else {
//             worker->savepoint_count[i] = 1;
//           }
//         } else {
//           return -1;
//         }
//       }
//       ret = write_keyval_pair(worker, k, strlen(k), v, strlen(v), i);
//       return ret;
//     } else {
//       lower_bound += (MAX_NUM_KEYS / num_partitions);
//     }
//   }
//   return -1;
// }

int handle_write(struct datastore_env_params *rocks, char *key, int db_id) {
  char *k = strtok(key, " ");
  char *v = strtok(NULL, " ");
  int ret = write_keyval_pair(rocks, k, strlen(k), v, strlen(v), db_id);
  return ret;
}

// int handle_write(struct datastore_worker_params *worker, char *key) {
//   char *k = strtok(key, " ");
//   char *v = strtok(NULL, " ");
//   int ret =
//       write_keyval_pair(worker, k, strlen(k), v, strlen(v),
//       worker->worker_id);
//   return ret;
// }

int handle_multidb_setsavepoint(struct datastore_env_params *rocks) {
  for (int i = 0; i < rocks->num_workers; i++) {
    if (rocks->savepoint_count[i] >
        0) { // if this count > 0, then a tx should exist
      if (rocks->tx_db[i] == NULL) {
        printf("Database is not open\n");
        return -1;
      }
      if (rocks->tx[i] == NULL) {
        printf("Begin a transaction first\n");
        return -1;
      }
      rocksdb_transaction_set_savepoint(rocks->tx[i]);
      rocks->savepoint_count[i]++;
    }
  }
  return 0;
}
//
// int handle_multidb_setsavepoint(struct datastore_worker_params *worker,
//                                 int num_partitions) {
//   for (int i = 0; i < num_partitions; i++) {
//     if (worker->savepoint_count[i] >
//         0) { // if this count > 0, then a tx should exist
//       if (worker->tx_db[i] == NULL) {
//         printf("Database is not open\n");
//         return -1;
//       }
//       if (worker->tx[i] == NULL) {
//         printf("Begin a transaction first\n");
//         return -1;
//       }
//       rocksdb_transaction_set_savepoint(worker->tx[i]);
//       worker->savepoint_count[i]++;
//     }
//   }
//   return 0;
// }

int handle_multidb_rollback(struct datastore_env_params *rocks) {
  for (int i = 0; i < rocks->num_workers; i++) {
    if (rocks->savepoint_count[i] >
        0) { // if this count > 0, then a tx should exist
      if (rocks->tx_db[i] == NULL) {
        printf("Database is not open\n");
        return -1;
      }
      if (rocks->tx[i] == NULL) {
        printf("Begin a transaction first\n");
        return -1;
      }
      char *err = NULL;
      rocksdb_transaction_rollback_to_savepoint(rocks->tx[i], &err);
      if (err != NULL) {
        printf("Error in rolling back to savepoint\n");
        printf("%s\n", err);
        return -1;
      }
    }
    rocks->savepoint_count[i]--;
  }
  return 0;
}

// int handle_multidb_rollback(struct datastore_worker_params *worker,
//                             int num_partitions) {
//   for (int i = 0; i < num_partitions; i++) {
//     if (worker->savepoint_count[i] >
//         0) { // if this count > 0, then a tx should exist
//       if (worker->tx_db[i] == NULL) {
//         printf("Database is not open\n");
//         return -1;
//       }
//       if (worker->tx[i] == NULL) {
//         printf("Begin a transaction first\n");
//         return -1;
//       }
//       char *err = NULL;
//       rocksdb_transaction_rollback_to_savepoint(worker->tx[i], &err);
//       if (err != NULL) {
//         printf("Error in rolling back to savepoint\n");
//         printf("%s\n", err);
//         return -1;
//       }
//     }
//     worker->savepoint_count[i]--;
//   }
//   return 0;
// }
/**
 * @brief
 *
 */
void init_database_env(struct datastore_env_params *rocksdb_env) {
  rocksdb_env->options = rocksdb_options_create();
  rocksdb_env->txdb_options = rocksdb_transactiondb_options_create();
  rocksdb_env->write_options = rocksdb_writeoptions_create();
  rocksdb_env->read_options = rocksdb_readoptions_create();
  rocksdb_env->tx_options = rocksdb_transaction_options_create();
  rocksdb_options_set_create_if_missing(rocksdb_env->options, 1);
  rocksdb_env->db_path[0] = "0";
  rocksdb_env->db_path[1] = "1";
  rocksdb_env->db_path[2] = "2";
  rocksdb_env->db_path[3] = "3";

  for (int i = 0; i < MAX_NB_PARTITION; i++) {
    rocksdb_env->savepoint_count[i] = 0;
  }

  uint32_t num_workers = app_get_lcores_worker();
  rocksdb_env->num_workers = (int)num_workers;
  for (int i = 0; i < (int)num_workers; i++) {
    rocksdb_env->workers[i].worker_id = i;
    // rocksdb_env->workers[i].db_path[0] = "0";
    // rocksdb_env->workers[i].db_path[1] = "1";
    // rocksdb_env->workers[i].db_path[2] = "2";
    // rocksdb_env->workers[i].db_path[3] = "3";

    // for (int j = 0; j < MAX_NB_PARTITION; j++) {
    //   rocksdb_env->workers[i].savepoint_count[j] = 0;
    // }
    // int ret = open_database(&rocksdb_env->workers[i], num_workers,
    //                         rocksdb_env->options, rocksdb_env->txdb_options);
    // if (ret < 0) {
    //   printf("Unable to open db\n");
    // }
  }

  for (int i = 0; i < 4; i++) {
    int ret = open_database(rocksdb_env, i);
    if (ret < 0) {
      printf("Unable to open db\n");
    }
  }
}

/**
 * @brief extern ROCKSDB_LIBRARY_API rocksdb_transactiondb_t*
 rocksdb_transactiondb_open( const rocksdb_options_t* options, const
 rocksdb_transactiondb_options_t* txn_db_options, const char* name, char**
 errptr);
 *
 */
int open_database(struct datastore_env_params *rocks, int id) {
  char *err = NULL;

  rocks->tx_db[id] = rocksdb_transactiondb_open(
      rocks->options, rocks->txdb_options, rocks->db_path[id], &err);
  if (err != NULL) {
    printf("An error occurred while creating the database %s \n",
           rocks->db_path[id]);
    printf("%s\n", err);
    return -1;
  }

  return 0;

  // for (int i = 0; i < num_workers; i++) {
  //   worker->tx_db[i] = rocksdb_transactiondb_open(db_options, txdb_options,
  //                                                 worker->db_path[i], &err);
  //   if (err != NULL) {
  //     printf("An error occurred while creating the database %s \n",
  //            worker->db_path[i]);
  //     printf("%s\n", err);
  //     return -1;
  //   }
  // }
  // return 0;
}
// int open_database(struct datastore_worker_params *worker, int num_workers,
//                   rocksdb_options_t *db_options,
//                   rocksdb_transactiondb_options_t *txdb_options) {
//   char *err = NULL;
//   for (int i = 0; i < num_workers; i++) {
//     worker->tx_db[i] = rocksdb_transactiondb_open(db_options, txdb_options,
//                                                   worker->db_path[i], &err);
//     if (err != NULL) {
//       printf("An error occurred while creating the database %s \n",
//              worker->db_path[i]);
//       printf("%s\n", err);
//       return -1;
//     }
//   }
//   return 0;
// }

/**
 * @brief extern ROCKSDB_LIBRARY_API void rocksdb_transactiondb_close(
    rocksdb_transactiondb_t* txn_db);
 *
 * @return int
 */
int close_database(struct datastore_env_params *rocks, int db_id) {
  if (rocks->tx_db[db_id] == NULL) // check on the actual database not the name
  {
    printf("Database not open\n");
    return 0; // db doesnt exist
  }
  if (rocks->tx[db_id] != NULL) {
    printf("Transaction will be aborted\n");
  }
  rocksdb_transactiondb_close(rocks->tx_db[db_id]);
  rocks->tx_db[db_id] = NULL;
  rocks->db_path[db_id] = NULL;
  rocks->tx[db_id] = NULL;
  printf("Database closed successfully \n");
  return 0;
}

// int close_database(struct datastore_worker_params *worker, int num_workers,
//                    int db_id) {
//   if (worker->tx_db[db_id] == NULL) // check on the actual database not the
//   name
//   {
//     printf("Database not open\n");
//     return 0; // db doesnt exist
//   }
//   if (worker->tx[db_id] != NULL) {
//     printf("Transaction will be aborted\n");
//   }
//   rocksdb_transactiondb_close(worker->tx_db[db_id]);
//   worker->tx_db[db_id] = NULL;
//   worker->db_path[db_id] = NULL;
//   worker->tx[db_id] = NULL;
//   printf("Database closed successfully \n");
//   return 0;
// }

void deconstruct_database_env(struct datastore_env_params *rocksdb_env) {
  if (rocksdb_env->txdb_options) {
    rocksdb_transactiondb_options_destroy(rocksdb_env->txdb_options);
    rocksdb_env->txdb_options = NULL;
  }
  if (rocksdb_env->options) {
    rocksdb_options_destroy(rocksdb_env->options);
    rocksdb_env->options = NULL;
  }
  if (rocksdb_env->write_options) {
    rocksdb_writeoptions_destroy(rocksdb_env->write_options);
    rocksdb_env->write_options = NULL;
  }
  if (rocksdb_env->read_options) {
    rocksdb_readoptions_destroy(rocksdb_env->read_options);
    rocksdb_env->read_options = NULL;
  }
  if (rocksdb_env->tx_options) {
    rocksdb_transaction_options_destroy(rocksdb_env->tx_options);
    rocksdb_env->tx_options = NULL;
  }
}

/**
 * @brief delete's the database and removes the folder
 * we have to provide the name, because when we close the database, we set
 * worker.db_path to NULL and free the memory, so we have to make sure we are
 * deleting the right database
 * @return int
 */
int destroy_database(rocksdb_options_t *options, char *db_path) {
  char *err = NULL;
  rocksdb_destroy_db(options, db_path, &err);
  if (err != NULL) {
    printf("Cannot delete database\n");
    printf("%s\n", err);
    return -1;
  }

  return 0;
}

/**
 * @brief extern ROCKSDB_LIBRARY_API rocksdb_transaction_t*
 rocksdb_transaction_begin( rocksdb_transactiondb_t* txn_db, const
 rocksdb_writeoptions_t* write_options, const rocksdb_transaction_options_t*
 txn_options, rocksdb_transaction_t* old_txn);
 *
 * @param worker
 * @return int
 */
int begin_transaction(struct datastore_env_params *rocks, int db_id) {
  if (rocks->tx_db[db_id] == NULL) {
    fprintf(stderr, "Database %s is not open for begin tx\n",
            rocks->db_path[db_id]);
    return -1;
  }
  if (rocks->tx[db_id] != NULL) // REVIEW: should we reuse the transaction?
  {
    // TODO: keep transactions in a FIFO queue
    fprintf(stderr, "Another transaction is in process\n");
    return -1;
  }
  // FIXME: function called returns a pointer to a transaction, we're storing it
  // in the struct itself
  rocks->tx[db_id] = rocksdb_transaction_begin(
      rocks->tx_db[db_id], rocks->write_options, rocks->tx_options, NULL);
  return 0;
}

// int begin_transaction(struct datastore_worker_params *worker,
//                       rocksdb_writeoptions_t *write_options,
//                       rocksdb_transaction_options_t *tx_options, int db_id) {
//   if (worker->tx_db[db_id] == NULL) {
//     fprintf(stderr, "Database %s is not open for begin tx\n",
//             worker->db_path[db_id]);
//     return -1;
//   }
//   if (worker->tx[db_id] != NULL) // REVIEW: should we reuse the transaction?
//   {
//     // TODO: keep transactions in a FIFO queue
//     fprintf(stderr, "Another transaction is in process\n");
//     return -1;
//   }
//   // FIXME: function called returns a pointer to a transaction, we're storing
//   it
//   // in the struct itself
//   worker->tx[db_id] = rocksdb_transaction_begin(
//       worker->tx_db[db_id], write_options, tx_options, NULL);
//   return 0;
// }
/**
 * @brief extern ROCKSDB_LIBRARY_API void rocksdb_transaction_commit(
    rocksdb_transaction_t* txn, char** errptr);
 *
 * @param worker
 * @return int
 */
int commit_transaction(struct datastore_env_params *rocks, int db_id) {
  char *err = NULL;
  if (rocks->tx_db[db_id] == NULL) {
    printf("Database is not open for commit\n");
    return -1;
  }
  if (rocks->tx[db_id] == NULL) {
    printf("No transaction to commit\n");
    return -1;
  }
  rocksdb_transaction_commit(rocks->tx[db_id], &err);
  if (err != NULL) {
    printf("An error occured while attempting to commit transaction \n");
    printf("%s\n", err);
    return -1;
  }
  printf("Transaction committed successfully\n");
  rocks->tx[db_id] = NULL; // REVIEW: should we reuse the transaction
  return 0;
}

// int commit_transaction(struct datastore_worker_params *worker, int db_id) {
//   char *err = NULL;
//   if (worker->tx_db[db_id] == NULL) {
//     printf("Database is not open for commit\n");
//     return -1;
//   }
//   if (worker->tx[db_id] == NULL) {
//     printf("No transaction to commit\n");
//     return -1;
//   }
//   rocksdb_transaction_commit(worker->tx[db_id], &err);
//   if (err != NULL) {
//     printf("An error occured while attempting to commit transaction \n");
//     printf("%s\n", err);
//     return -1;
//   }
//   printf("Transaction committed successfully\n");
//   worker->tx[db_id] = NULL; // REVIEW: should we reuse the transaction
//   return 0;
// }

int delete_transaction(struct datastore_env_params *rocks, int db_id) {
  if (rocks->tx_db[db_id] == NULL) {
    printf("Database is not open for delete\n");
    return -1;
  }
  if (rocks->tx[db_id] == NULL) {
    printf("No transaction to delete\n");
    return 0;
  }
  rocksdb_transaction_destroy(rocks->tx[db_id]);
  rocks->tx[db_id] = NULL;
  return 0;
}

// int delete_transaction(struct datastore_worker_params *worker, int db_id) {
//   if (worker->tx_db[db_id] == NULL) {
//     printf("Database is not open for delete\n");
//     return -1;
//   }
//   if (worker->tx[db_id] == NULL) {
//     printf("No transaction to delete\n");
//     return 0;
//   }
//   rocksdb_transaction_destroy(worker->tx[db_id]);
//   worker->tx[db_id] = NULL;
//   return 0;
// }
int write_keyval_pair(struct datastore_env_params *rocks, char *key,
                      size_t keylen, char *val, size_t vallen, int db_id) {
  if (rocks->tx_db[db_id] == NULL) {
    printf("Database is not open for write\n");
    return -1;
  }
  if (rocks->tx[db_id] == NULL) {
    printf("Begin a transaction first\n");
    return -1;
  }
  char *err = NULL;
  rocksdb_transaction_put(rocks->tx[db_id], key, keylen, val, vallen, &err);
  if (err != NULL) {
    printf("%s %s, %s\n", "Error writing key value pair", key, val);
    printf("%s\n", err);
    return -1;
  }
  printf("Key: %s, Value: %s pair was written successfully\n", key, val);
  return 0;
}

// int write_keyval_pair(struct datastore_worker_params *worker, char *key,
//                       size_t keylen, char *val, size_t vallen, int db_id) {
//   if (worker->tx_db[db_id] == NULL) {
//     printf("Database is not open for write\n");
//     return -1;
//   }
//   if (worker->tx[db_id] == NULL) {
//     printf("Begin a transaction first\n");
//     return -1;
//   }
//   char *err = NULL;
//   rocksdb_transaction_put(worker->tx[db_id], key, keylen, val, vallen, &err);
//   if (err != NULL) {
//     printf("%s %s, %s\n", "Error writing key value pair", key, val);
//     printf("%s\n", err);
//     return -1;
//   }
//   printf("Key: %s, Value: %s pair was written successfully\n", key, val);
//   return 0;
// }

int read_keyval_pair(struct datastore_env_params *rocks, char *key,
                     size_t keylen, int db_id) {
  if (rocks->tx_db[db_id] == NULL) {
    fprintf(stderr, "Database is not open for read\n");
    return -1;
  }
  if (rocks->tx[db_id] == NULL) {
    fprintf(stderr, "Begin a transaction first\n");
    return -1;
  }
  char *err = NULL;
  char *val = NULL;
  size_t vallen;
  val = rocksdb_transaction_get(rocks->tx[db_id], rocks->read_options, key,
                                keylen, &vallen, &err);
  if (err != NULL) {
    fprintf(stderr, "There was an error reading the value\n");
    printf("%s\n", err);
    return -1;
  }
  if (val == NULL) {
    fprintf(stderr, "Value was not found for key %s\n", key);
    return -1; // REVIEW: should this be return 0?
  }
  fprintf(stdout, "Value for key %s is %s\n", key, val);
  free(val);
  return 0;
}
//
// int read_keyval_pair(struct datastore_worker_params *worker,
//                      rocksdb_readoptions_t *read_options, char *key,
//                      size_t keylen, int db_id) {
//   if (worker->tx_db[db_id] == NULL) {
//     fprintf(stderr, "Database is not open for read\n");
//     return -1;
//   }
//   if (worker->tx[db_id] == NULL) {
//     fprintf(stderr, "Begin a transaction first\n");
//     return -1;
//   }
//   char *err = NULL;
//   char *val = NULL;
//   size_t vallen;
//   val = rocksdb_transaction_get(worker->tx[db_id], read_options, key, keylen,
//                                 &vallen, &err);
//   if (err != NULL) {
//     fprintf(stderr, "There was an error reading the value\n");
//     printf("%s\n", err);
//     return -1;
//   }
//   if (val == NULL) {
//     fprintf(stderr, "Value was not found for key %s\n", key);
//     return -1; // REVIEW: should this be return 0?
//   }
//   fprintf(stdout, "Value for key %s is %s\n", key, val);
//   free(val);
//   return 0;
// }

/**
 * @brief delete a key from the database.
 * the transaction must be committed for the key to be deleted from the
 * database.
 */
int delete_keyval_pair(struct datastore_env_params *rocks, char *key,
                       size_t keylen, int db_id) {
  if (rocks->tx_db[db_id] == NULL) {
    printf("Database is not open\n");
    return -1;
  }
  if (rocks->tx[db_id] == NULL) {
    printf("Begin a transaction first\n");
    return -1;
  }

  char *err = NULL;
  rocksdb_transaction_delete(rocks->tx[db_id], key, keylen, &err);
  if (err != NULL) {
    printf("There was an error deleting the entry\n");
    printf("%s\n", err);
    return -1;
  }
  return 0;
}
//
// int delete_keyval_pair(struct datastore_worker_params *worker, char *key,
//                        size_t keylen, int db_id) {
//   if (worker->tx_db[db_id] == NULL) {
//     printf("Database is not open\n");
//     return -1;
//   }
//   if (worker->tx[db_id] == NULL) {
//     printf("Begin a transaction first\n");
//     return -1;
//   }
//
//   char *err = NULL;
//   rocksdb_transaction_delete(worker->tx[db_id], key, keylen, &err);
//   if (err != NULL) {
//     printf("There was an error deleting the entry\n");
//     printf("%s\n", err);
//     return -1;
//   }
//   return 0;
// }

int set_savepoint(struct datastore_env_params *rocks, int db_id) {
  if (rocks->tx_db[db_id] == NULL) {
    printf("Database is not open\n");
    return -1;
  }
  if (rocks->tx[db_id] == NULL) {
    printf("Begin a transaction first\n");
    return -1;
  }
  rocksdb_transaction_set_savepoint(rocks->tx[db_id]);
  return 0;
}

// int set_savepoint(struct datastore_worker_params *worker, int db_id) {
//   if (worker->tx_db[db_id] == NULL) {
//     printf("Database is not open\n");
//     return -1;
//   }
//   if (worker->tx[db_id] == NULL) {
//     printf("Begin a transaction first\n");
//     return -1;
//   }
//   rocksdb_transaction_set_savepoint(worker->tx[db_id]);
//   return 0;
// }
int rollback_to_last_savepoint(struct datastore_env_params *rocks, int db_id) {
  if (rocks->tx_db[db_id] == NULL) {
    printf("Database is not open\n");
    return -1;
  }
  if (rocks->tx[db_id] == NULL) {
    printf("Begin a transaction first\n");
    return -1;
  }
  char *err = NULL;
  rocksdb_transaction_rollback_to_savepoint(rocks->tx[db_id], &err);
  if (err != NULL) {
    printf("Error in rolling back to savepoint\n");
    printf("%s\n", err);
    return -1;
  }
  return 0;
}

// int rollback_to_last_savepoint(struct datastore_worker_params *worker,
//                                int db_id) {
//   if (worker->tx_db[db_id] == NULL) {
//     printf("Database is not open\n");
//     return -1;
//   }
//   if (worker->tx[db_id] == NULL) {
//     printf("Begin a transaction first\n");
//     return -1;
//   }
//   char *err = NULL;
//   rocksdb_transaction_rollback_to_savepoint(worker->tx[db_id], &err);
//   if (err != NULL) {
//     printf("Error in rolling back to savepoint\n");
//     printf("%s\n", err);
//     return -1;
//   }
//   return 0;
// }
