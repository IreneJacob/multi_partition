#ifndef MAX_HMAP_ELEMENTS
#define MAX_HMAP_ELEMENTS 10
#endif

typedef struct node{
  char * key;
  int hash_index;
  int pos;
} node;

typedef struct hashmap{
  int size;
  node ** node_list;
  // int capacity;
  int checkpoint;
} hashmap;

// Forward declarations
hashmap * create_map();
int destroy_map(hashmap * h);
node * create_node(int key_size, char * k);
int map_insert_node(hashmap * h, char * k);
int map_remove_node(hashmap * h, char * key);
node * map_lookup_node(hashmap * h, char * key);
int hash(char * key);
int map_set_checkpoint(hashmap * h);
int map_rollback_to_checkpoint(hashmap * h);
int handle_map_insert(hashmap * h, char * key);
