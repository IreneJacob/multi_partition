#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>

#include "hashmap.h"

// Forward declarations
int handle_map_insert(hashmap * h, char * key); // insert a key into the hashmap and return its hash index



// create an empty hashmap
hashmap * create_map(){
  hashmap * map = malloc (sizeof (hashmap));
  if (map == NULL) {
    return NULL;
  }
  map->node_list = malloc(MAX_HMAP_ELEMENTS * sizeof(node));
  if (map->node_list == NULL) {
    free(map);
    return NULL;
  }
  map->size = 0;
  map->checkpoint = 0;
  return map;
}

// free space allocated to hashmap
int destroy_map(hashmap * h){
  if (h == NULL) {
    // printf("%s\n", "No such hashmap found");
    return 0;
  }
  int num_elements = h->size - 1;
  /* map_remove_node changes the size parameter of the hashmap, so we have to traverse the hashmap
   from the end to the beginning and store the size of the hashmap before elements are removed in num_elements */
  for (int i = num_elements; i >= 0; i--) {
    node * n = h->node_list[i];
    if (n != NULL) {
      int j = map_remove_node(h, n->key);
      if (j < 0) {
        printf("%s %i\n", "An error occured in removing node", i);
      }
    }
  }
  h->size = 0;
  free(h->node_list);
  free(h);
  return 0;
}

// create a node with a key and a hash index
node * create_node(int key_size, char * k){
  node * n = malloc(sizeof(node));
  if (n == NULL) {
    printf("%s\n", "Could not create node" );
    return NULL;
  }
  n->key = malloc(key_size * sizeof(char));
  if (n->key == NULL) {
    printf("%s\n", "An error occured in creating node" );
    free(n);
    return NULL;
  }
  strcpy(n->key, k);
  n->hash_index = hash(k);
  n->pos = -1;
  return n;
}

// insert a key into the hashmap
int map_insert_node(hashmap * h, char * k){
  if (h == NULL) {
    printf("%s\n", "Hashmap not found" );
    return -1;
  }
  node * n = create_node(sizeof(k), k);
  if (n == NULL) {
    printf("%s\n", "Error creating node");
    return -1;
  }
  if (h->size < MAX_HMAP_ELEMENTS) {
    n->pos = h->size;
    h->node_list[h->size] = n;
    h->size = h->size +1;
  }
  return 0;
}

// remove a key from the hashmap
int map_remove_node(hashmap * h, char * key){
  if (h == NULL) {
    printf("%s\n", "Hashmap not found" );
    return -1;
  }
  node * n = map_lookup_node(h, key);
  if (n == NULL) {
    // printf("%s\n", "INFO: Trying to remove a non-existant node" );
    return 0;
  }
  int pos = n->pos;
  free(n->key);
  free(n);
  h->node_list[pos] = NULL;
  h->size = h->size -1;
  return 0;
}

// find the node with the key we are looking for
node * map_lookup_node(hashmap * h, char * key){
  for (int i = 0; i < h->size; i++) {
    node * n = h->node_list[i];
    if (strcmp(key,n->key) == 0) {
      return n;
    }
  }
  // printf("%s %s %s\n", "node with key ", key, " not found" );
  return NULL;
}

// hash function
int hash(char * key){
  return 0;
}

// saves a point up to which the keys are saved
int map_set_checkpoint(hashmap * h){
  if (h == NULL) {
    printf("%s\n", "Hashmap not found" );
    return -1;
  }
  h->checkpoint = h->size;
  return 0;
}

int map_rollback_to_checkpoint(hashmap * h){
  if (h == NULL) {
    printf("%s\n", "Hashmap not found" );
    return -1;
  }
  if (h->checkpoint > h->size) {
    printf("%s\n", "An error occured during rollback" );
    return -1;
  }
  for (int i = h->checkpoint; i < h->size; i++) {
    node * n = h->node_list[i];
    int j = map_remove_node(h, n->key);
    if (j < 0) {
      printf("%s\n", "An error occured during rollback");
    }
  }
  return 0;
}

int handle_map_insert(hashmap * h, char * key){
  if (h == NULL) {
    printf("%s\n", "Hashmap not found");
    return -1;
  }
  // handle the case when key was previously stored
  node * n = map_lookup_node(h, key);
  if (n != NULL) {
    return n->hash_index;
  }
  // else create a new node and return the hash index
  int ret = map_insert_node(h, key);
  if (ret < 0) {
    printf("%s\n", "Error inserting key in hashmap" );
    return -1;
  }
  /* the node we inserted would be at the end of the node_list in the hashmap,
   so we avoid looping through the list with map_lookup_node and simply look at the last node*/
  n = h->node_list[h->size - 1];
  return n->hash_index;

}
