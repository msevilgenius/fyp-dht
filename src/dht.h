#ifndef DHT_H
#define DHT_H

#include <stdint.h>

#define hash_type uint32_t
#define FINGER_SIZE_INIT 6

struct dht_node{
    hash_type id;
    uint32_t IP;
    uint16_t port;
};

struct node_data* dht_node_create();

void dht_node_destroy(struct node_data* n);

/**
 * creates a new DHT network
 *
 */
int dht_create(struct dht_node* self);


/**
 * join an existing DHT network
 *
 */
int dht_join(struct dht_node* self, struct dht_node* node);

/**
 * find successor of id
 */
hash_type dht_find_successor(struct dht_node* self, hash_type id);

/**
 * ask node n for the successor of id
 */
hash_type dht_find_successor_remote(struct dht_node* self, struct dht_node* n, hash_type id);

/**
 * find highest known predecessor of id
 */
hash_type dht_closest_preceding_node(struct dht_node* self, hash_type id);

/**
 *
 * called periodically
 */
void dht_stabalize(struct dht_node* self);

/**
 * notify node_id of self's existence
 * self believes node_id is it's successor
 */
void dht_notify(struct dht_node* self, hash_type node_id);

/**
 *
 * called periodically
 */
void dht_fix_fingers(struct dht_node* self);

/**
 * checks whether predecessor is still alive, and updates internal state
 * called periodically
 */
void dht_check_predecessor(struct dht_node* self);

/**
 * called when a message from another node is received
 */
void handle_message(struct dht_node* from, char** message);


int dht_send_message(struct dht_node* self, char** message, hash_type to);

#endif // DHT_H
