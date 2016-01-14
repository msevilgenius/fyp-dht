#include "dht.h"
#include "netio.h"

#include <string.h>

#define MSG_T_NOTIF     1
#define MSG_T_SUCC_REP  2
#define MSG_T_SUCC_REQ  3
#define MSG_T_PRED_REP  4
#define MSG_T_PRED_REQ  5
#define MSG_T_ALIVE_REP 6
#define MSG_T_ALIVE_REQ 7

struct node_data{
    hash_type id;
    dht_node successor;
    dht_node predecessor;
    short has_pred;
    struct dht_node* finger_table;
    struct net_server* net;
};

struct node_data* dht_node_create()
{
    struct node_data* node = malloc(sizeof(struct node_data));
    if(!node) { return NULL; }

    node->id = 0;
    node->predecessor = {0, 0, 0};
    node->successor = {0, 0, 0};

    node->finger_table = malloc(sizeof(dht_node) * ID_BITS);
    if (!node->finger_table){
            free(node);
            return NULL; }

    node->net = net_server_create();
    if (!node->net){
            free(node->finger_table);
            free(node);
            return NULL; }

    return node;
}

void dht_node_destroy(struct node_data* n)
{
    if (!n) { return; }
    if (n->net){ net_server_destroy(n->net); }
    if (n->finger_table){ free(n->finger_table); }
    free(n);
}

int dht_create(struct node_data* self)
{
    int rv = net_server_run(self->net);
    if (rv) return -1;

    n->predecessor = self->id;
    n->has_pred = 1;
    return 0;
}

int dht_join(struct node_data* self, dht_node node)
{
    self->has_pred = 0; // nil
    self->successor = dht_find_successor_remote(self, node, self->id);
    return 0;
}

hash_type dht_find_successor(struct node_data* self, hash_type id)
{

}

hash_type dht_find_successor_remote(struct node_data* self, dht_node n, hash_type id)
{

}

int dht_send_message(struct node_data* self, dht_message* message)
{

}

hash_type dht_closest_preceding_node(struct node_data* self, hash_type id)
{
    for (hash_type i = ID_BITS-1; i >= 0; --i){
        hash_type n = self->finger_table[i]->id;
        if (dht_id_in_range(n, self->id, id)){
            return (n->id);
        }
    }
    return (self->id);
}

void dht_stabalize(struct node_data* self)
{

}

void dht_notify(struct node_data* self, hash_type node_id)
{
    char msg[100];
    snprintf(msg, 100, "%X\n%X\n%s%s", self->id, node_id, REQ_NOTIFY, MES_END);
    dht_send_message(self, msg, node_id);
}

void dht_notified(struct node_data* self, struct dht_node node)
{
    if (!self->has_pred || dht_id_in_range(node->id, self->predecessor, self->id)){
        self->predecessor = node;
        self->has_pred = 1;
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

void dht_fix_fingers(struct node_data* self)
{

}

void dht_check_predecessor(struct node_data* self)
{
    if (!self->has_pred){ return; }
    char msg[100];
    snprintf(msg, 100, "%X\n%X\n%s%s", self->id, self->predecessor, REQ_ALIVE, MES_END);
    dht_send_message(self, msg, node_id);
}

void handle_message(struct node_data* self, struct dht_message* message, struct evbuffer *replyto)
{
    struct dht_node other;
    switch (msg->type){

        case MSG_T_SUCC_REQ:
            break;
        case MSG_T_PRED_REQ:
            break;
        case MSG_T_ALIVE_REQ:
            evbuffer_add_printf(replyto, MSG_FMT, msg->to, msg->from, REP_ALIVE, 0);
            break;
        case MSG_T_NOTIF:
            other.id = message->from;
            // TODO parse content -> other
            dht_notified(self, other);
            break;

        case MSG_T_SUCC_REP:
            break;
        case MSG_T_PRED_REP:
            break;
        case MSG_T_ALIVE_REP:
            break;
    }

    if (msg->len > 0){
        free(msg->content);}
    return;
}

int parse_msg_type(const char* typestr)
{
    if (strcmp(typestr, REQ_SUCCESSOR) == 0){
        return MSG_T_SUCC_REQ;
    }
    if (strcmp(typestr, REP_SUCCESSOR) == 0){
        return MSG_T_SUCC_REP;
    }
    if (strcmp(typestr, REQ_PREDESSOR) == 0){
        return MSG_T_PRED_REQ;
    }
    if (strcmp(typestr, REP_PREDESSOR) == 0){
        return MSG_T_PRED_REP;
    }
    if (strcmp(typestr, REQ_ALIVE) == 0){
        return MSG_T_ALIVE_REQ;
    }
    if (strcmp(typestr, REP_ALIVE) == 0){
        return MSG_T_ALIVE_REP;
    }
    if (strcmp(typestr, REQ_NOTIFY) == 0){
        return MSG_T_NOTIF;
    }
    return -1;
}

void recv_message(struct bufferevent *bev, void *arg)
{
    struct node_data* self = (struct node_data*) arg;
    struct dht_message msg;
    struct evbuffer *input = bufferevent_get_input(bev);
    struct evbuffer *output = bufferevent_get_output(bev);
    int header_len = 0;
    int line_len;

    char* endptr;
    char* saveptr;
    char* token;

    // from id
    token = evbuffer_readln(input, &line_len, EVBUFFER_EOL_LF);
    msg.from.id = strtoull(token, &endptr, 16);
    header_len += line_len + 1;
    free(token);

    // to id
    token = evbuffer_readln(input, &line_len, EVBUFFER_EOL_LF);
    msg.to.id = strtoull(token, &endptr, 16);
    header_len += line_len + 1;
    free(token);

    // type of message
    token = evbuffer_readln(input, &line_len, EVBUFFER_EOL_LF);
    if (msg.type = parse_msg_type(token) == -1) return;
    header_len += line_len + 1;
    free(token);

    // length of content or 0 if none
    token = evbuffer_readln(input, &line_len, EVBUFFER_EOL_LF);
    msg.len = (int) strtoul(token, &endptr, 10);
    header_len += line_len + 1;
    free(token);

    if (msg.len > 0){
        msg.content = malloc(sizeof(char)*msg.len);
        int copied = evbuffer_remove(input, msg.content, msg.len);
        if (copied == -1) {
            // TODO error reading
        }
        if (copied < message.len){
            // TODO not enough data
        }
    }else{
        msg.content = NULL;
    }

    handle_message(self, &msg, output);
}
















