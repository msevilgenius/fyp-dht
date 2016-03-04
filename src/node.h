#ifndef DHT_H
#define DHT_H

#include <stdint.h>
#include <event2/buffer.h>

#include "libdht.h"
#include "netio.h"

#define ID_BITS 32
#define ID_HEX_CHARS (ID_BITS/4)
#define FINGER_SIZE_INIT 6
// wait 20 secs before timout node
#define NODE_TIMEOUT 20
#define STABILIZE_PERIOD 90

struct node_self;

struct node_found_cb_data;
struct node_join_cb_data;
struct node_found_cb_data;
struct finger_update_arg;

struct node_message{
    struct node_info to;
    struct node_info from;
    int type;
    int len;
    char* content;
};

typedef void (*on_join_cb_t)(void* arg);
typedef void (*node_found_cb_t)(struct node_info, void *);
typedef void (*node_msg_cb_t)(struct node_self*, struct node_message*, int, void *);

struct node_self* node_create(uint16_t listen_port, char* name);

void node_destroy(struct node_self* n);

struct net_server* node_get_net(struct node_self* self);

/**
 * creates a new overlay network
 *
 */
int node_network_create(struct node_self* self, on_join_cb_t join_cb, void *arg);

void node_set_node_msg_handler(struct node_self* self, node_msg_cb_t, void* cb_arg);


/**
 * join an existing overlay network
 *
 */
int node_network_join(struct node_self* self, struct node_info node, on_join_cb_t join_cb, void * cb_arg);

/**
 * find successor of id
 */
int node_find_successor(struct node_self* self, hash_type id, node_found_cb_t cb, void* found_cb_arg);

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

void node_alive_timeout(struct node_self* self, struct node_info* node);

/**
 * called when a message from another node is received
 */
void handle_message(struct node_self* self, struct node_message* message, int connection);


int node_send_message(struct node_self* self, struct node_message* message, const int connection);

int node_connect_and_send_message(struct node_self* self, struct node_message* msg,
                                  net_connection_data_cb_t read_cb, net_connection_event_cb_t event_cb,
                                  void *cb_arg, const struct timeval *timeout);

#endif // DHT_H
