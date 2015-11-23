#ifndef DHT_H
#define DHT_H

#define hash_type uint32_t
#define FINGER_SIZE_INIT 6

struct dht_node{
    hash_type id;
    uint32_t IP;
    uint16_t port;
};

struct node_data* dht_node_create();

dht_node_destroy(struct node_data* n);

/**
 * creates a new DHT network
 *
 */
int dht_create(node_data self);


/**
 * join an existing DHT network
 *
 */
int dht_join(node_data self, dht_node node);

/**
 * ask node n for the successor of id
 */
hash_type dht_find_successor(node_data self, dht_node n, hash_type id);

/**
 * find highest known predecessor of id
 */
hash_type dht_closest_preceding_node(node_data self, hash_type id);

/**
 *
 * called periodically
 */
 void dht_stabalize(node_data self);

/**
 * notify node_id of self's existence
 * self believes node_id is it's successor
 */
 void dht_notify(node_data self, hash_type node_id);

/**
 *
 * called periodically
 */
 void dht_fix_fingers(node_data self);

/**
 * checks whether predecessor is still alive, and updates internal state
 * called periodically
 */
 void dht_check_predecessor(node_data self);

#endif // DHT_H
