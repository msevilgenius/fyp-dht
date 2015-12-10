#ifndef DHT_H
#define DHT_H

#include <stdint.h>

#define hash_type uint32_t
#define ID_BITS 32
#define ID_HEX_CHARS (ID_BITS/4)
#define FINGER_SIZE_INIT 6

struct node_data;

struct dht_node{
    hash_type id;
    uint32_t IP;
    uint16_t port;
};

struct dht_message{
    struct dht_node to;
    struct dht_node from;
    int type;
    int len;
    char* content;
};

struct node_data* dht_node_create();

void dht_node_destroy(struct node_data* n);

/**
 * creates a new DHT network
 *
 */
int dht_create(struct node_data* self);


/**
 * join an existing DHT network
 *
 */
int dht_join(struct node_data* self, struct dht_node* node);

/**
 * find successor of id
 */
hash_type dht_find_successor(struct node_data* self, hash_type id);

/**
 * ask node n for the successor of id
 */
hash_type dht_find_successor_remote(struct node_data* self, struct dht_node* n, hash_type id);

/**
 * find highest known predecessor of id
 */
hash_type dht_closest_preceding_node(struct node_data* self, hash_type id);

/**
 *
 * called periodically
 */
void dht_stabalize(struct node_data* self);

/**
 * notify node_id of self's existence
 * self believes node_id is it's successor
 */
void dht_notify(struct node_data* self, hash_type node_id);

/**
 *
 * called periodically
 */
void dht_fix_fingers(struct node_data* self);

/**
 * checks whether predecessor is still alive, and updates internal state
 * called periodically
 */
void dht_check_predecessor(struct node_data* self);

void dht_alive_timeout(struct node_data* self, struct dht_node* node);

/**
 * called when a message from another node is received
 */
void handle_message(struct node_data* from, char* message);


int dht_send_message(struct node_data* self, dht_message* message);

#endif // DHT_H
