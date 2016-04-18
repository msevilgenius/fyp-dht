#ifndef DHT_H
#define DHT_H

#include <stdint.h>
#include <event2/buffer.h>

#include "libdht.h"
#include "netio.h"

#define NUM_OF_SUCCS 8
#define ID_BITS 32
#define ID_BYTES (ID_BITS/8)
#define ID_HEX_CHARS (ID_BITS/4)
#define FINGER_SIZE_INIT 6
// wait 20 secs before timout node
#define NODE_TIMEOUT 20
#define STABILIZE_PERIOD 30
#define STABILIZE_CHECK_PERIOD 9


struct node_found_cb_data;
struct node_join_cb_data;
struct node_found_cb_data;
struct finger_update_arg;



/**
 * ask node n for the successor of id
 */
int node_find_successor_remote(struct node_self* self, struct node_info n, hash_type id, struct node_found_cb_data* cb_data);

/**
 * find highest known predecessor of id
 */
struct node_info node_closest_preceding_node(struct node_self* self, hash_type id);

/**
 * ask node n for its predecessor
 */
void node_get_predecessor_remote(struct node_self* self, struct node_info n, node_found_cb_t cb, void* found_cb_arg);

/**
 *
 * called periodically
 */
void node_network_stabalize(struct node_self* self);

/**
 * notify node_id of self's existence
 * self believes node_id is it's successor
 */
void node_notify_node(struct node_self* self, struct node_info node);

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

void node_check_successors(struct node_self* self);

void node_alive_timeout(struct node_self* self, struct node_info* node);

/**
 * called when a message from another node is received
 */
void handle_message(struct node_self* self, struct node_message* message, int connection);


/**
 * returns 2^n for positive n
 */
int two_to_the_n(int n);

#endif // DHT_H
