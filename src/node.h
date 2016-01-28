#ifndef DHT_H
#define DHT_H

#include <stdint.h>

#define hash_type uint32_t
#define ID_BITS 32
#define ID_HEX_CHARS (ID_BITS/4)
#define FINGER_SIZE_INIT 6

struct node_self;

struct node_info{
    hash_type id;
    uint32_t IP;
    uint16_t port;
};

struct node_message{
    struct node_info to;
    struct node_info from;
    int type;
    int len;
    char* content;
};

struct node_self* node_create();

void node_destroy(struct node_self* n);

/**
 * creates a new overlay network
 *
 */
int node_network_create(struct node_self* self);


/**
 * join an existing overlay network
 *
 */
int node_network_join(struct node_self* self, struct node_info* node);

/**
 * find successor of id
 */
hash_type node_find_successor(struct node_self* self, hash_type id);

/**
 * ask node n for the successor of id
 */
hash_type node_find_successor_remote(struct node_self* self, struct node_info* n, hash_type id);

/**
 * find highest known predecessor of id
 */
hash_type node_closest_preceding_node(struct node_self* self, hash_type id);

/**
 *
 * called periodically
 */
void node_network_stabalize(struct node_self* self);

/**
 * notify node_id of self's existence
 * self believes node_id is it's successor
 */
void node_notify_node(struct node_self* self, hash_type node_id);

/**
 *
 * called periodically
 */
void node_fix_fingers(struct node_self* self);

/**
 * checks whether predecessor is still alive, and updates internal state
 * called periodically
 */
void node_check_predecessor(struct node_self* self);

void node_alive_timeout(struct node_self* self, struct node_info* node);

/**
 * called when a message from another node is received
 */
void handle_message(struct node_self* from, char* message);


int node_send_message(struct node_self* self, node_message* message);

#endif // DHT_H