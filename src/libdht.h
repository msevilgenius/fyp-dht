#ifndef LIBDHT_H
#define LIBDHT_H

#include "dht.h"

// create node

// setup message handlers

/**
 *
 */
hash_type get_id(const char* name);

dht_node map_id_to_node(hash_type id);

int send_to(dht_node node, char* message);

int route_to(hash_type id, char* message);

#endif // LIBDHT_H
