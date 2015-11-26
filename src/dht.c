#include "dht.h"
#include "netio.h"


struct node_data{
    hash_type id;
    hash_type successor;
    hash_type predecessor;
    struct dht_node* finger_table;
};

struct node_data* dht_node_create()
{
    struct node_data* node = malloc(sizeof(struct node_data));

    node->id = 0;
    node->predecessor = 0;
    node->successor = 0;
    node->finger_table = NULL; //TODO

    return node;
}

void dht_node_destroy(struct node_data* n)
{
    free n;
}

int dht_create(node_data self)
{
    net_server_create
}

int dht_join(node_data self, dht_node node)
{
    self->predecessor = ; // nil
    self->successor = dht_find_successor(self, node, self->id);
}

hash_type dht_find_successor(node_data self, dht_node n, hash_type id)
{

}

hash_type dht_closest_preceding_node(node_data self, hash_type id)
{

}

void dht_stabalize(node_data self)
{

}

void dht_notify(node_data self, hash_type node_id)
{

}

void dht_fix_fingers(node_data self)
{

}

void dht_check_predecessor(node_data self)
{

}

void handle_message(dht_node* from, char** message)
{

}
