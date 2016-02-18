#ifndef LIBDHT_H
#define LIBDHT_H

#include "node.h"

// create node

// setup message handlers

/**
 *
 */
hash_type get_id(const char* name);

node_info map_id_to_node(hash_type id);

int send_to(node_info node, char* message);

int route_to(hash_type id, char* message);

/**
 * returns 2^n for positive n
 */
int two_to_the_n(int n);

#endif // LIBDHT_H
