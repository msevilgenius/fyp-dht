#ifndef LIBDHT_H
#define LIBDHT_H

#include <stdint.h>

typedef uint32_t hash_type;

struct node_info{
    hash_type id;
    uint32_t IP;
    uint16_t port;
};

// create node

// setup message handlers

/**
 *
 */
hash_type get_id(const char* name);

struct node_info map_id_to_node(hash_type id);

int send_to(struct node_info node, char* message);

int route_to(hash_type id, char* message);

/**
 * returns 2^n for positive n
 */
int two_to_the_n(int n);

#endif // LIBDHT_H
