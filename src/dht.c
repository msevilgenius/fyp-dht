#include "dht.h"
#include "netio.h"


struct node_data{
    hash_type id;
    hash_type successor;
    hash_type predecessor;
    short has_pred;
    struct dht_node* finger_table;
    struct net_server* net;
};

struct node_data* dht_node_create()
{
    struct node_data* node = malloc(sizeof(struct node_data));

    node->id = 0;
    node->predecessor = 0;
    node->successor = 0;
    node->finger_table = malloc(sizeof(dht_node) * (sizeof(hash_type)/8); //number of bits in id * size of node
    node->net = net_server_create();

    return node;
}

void dht_node_destroy(struct node_data* n)
{
    if (!n) { return; }
    if (n->net){ net_server_destroy(n->net); }
    if (n->finger_table){ free(n->finger_table); }
    free(n);
}

int dht_create(node_data self)
{
    net_server_run(self->net);
    n->predecessor = self->id;
}

int dht_join(node_data self, dht_node node)
{
    self->has_pred = 0; // nil
    self->successor = dht_find_successor_remote(self, node, self->id);
}

hash_type dht_find_successor(node_data self, hash_type id)
{

}

hash_type dht_find_successor_remote(node_data self, dht_node n, hash_type id)
{

}

dht_send_message(struct dht_node* self, char** message, hash_type to)
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
    char msg[100];
    snprintf(msg, 100, "f:%d\nt:%d\n%s%s", self->id, node_id, REQ_NOTIFY, MES_END);
    dht_send_message(self, msg, node_id);
}

void dht_notified(node_data self, hash_type node_id)
{
    if (!self->has_pred || dht_id_in_range(id, self->predecessor, self->id)){
        self->predecessor = node_id;
    }
}

void dht_id_in_range(hash_type id, hash_type min, hash_type max)
{
    if (max > min){ //easy
        return  ((id > min) && (id < max));
    }else if (max < min){ // min -> 0 -> max
        return  ((id > min) || ((id >= 0) && (id < max)));
    }else{ //max == min
        return (id == max);
    }
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
