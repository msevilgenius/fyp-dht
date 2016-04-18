#ifndef LIBDHT_H
#define LIBDHT_H

#include <stdint.h>

#include "libdhtnet.h"

typedef uint32_t hash_type;

struct node_self;

struct node_info{
    hash_type id;
    uint32_t IP;
    uint16_t port;
};

struct node_message{
    struct node_info to;
    struct node_info from;
    char type;
    uint32_t len;
    char* content;
};

typedef void (*on_join_cb_t)(void* arg);
typedef void (*node_found_cb_t)(struct node_info, void *, short);
typedef void (*node_msg_cb_t)(struct node_self*, struct node_message*, int, void *);

struct node_self* node_create(uint16_t listen_port, char* name);

void node_destroy(struct node_self* n);

struct net_server* node_get_net(struct node_self* self);

hash_type get_id(const char* name);

/**
 * creates a new overlay network
 */
int node_network_create(struct node_self* self, on_join_cb_t join_cb, void *arg);

void node_set_node_msg_handler(struct node_self* self, node_msg_cb_t, void* cb_arg);

/**
 * join an existing overlay network
 */
int node_network_join(struct node_self* self, struct node_info node, on_join_cb_t join_cb, void * cb_arg);

/**
 * find successor of id
 */
int node_find_successor(struct node_self* self, hash_type id, node_found_cb_t cb, void* found_cb_arg);

/**
 * send a message over a given connection opened from this nodes net_server
 */
int node_send_message(struct node_self* self, struct node_message* message, const int connection);

/**
 * create a connection to the given node and send a message over it
 */
int node_connect_and_send_message(struct node_self* self, struct node_message* msg,
                                  net_connection_data_cb_t read_cb, net_connection_event_cb_t event_cb,
                                  void *cb_arg, const struct timeval *timeout);

#endif // LIBDHT_H
