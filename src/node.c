#include "node.h"
#include "netio.h"

#include <string.h>

#define MSG_T_NOTIF     1
#define MSG_T_SUCC_REP  2
#define MSG_T_SUCC_REQ  3
#define MSG_T_PRED_REP  4
#define MSG_T_PRED_REQ  5
#define MSG_T_ALIVE_REP 6
#define MSG_T_ALIVE_REQ 7

struct node_self{
    hash_type id;
    node_info self;
    node_info successor;
    node_info predecessor;
    short has_pred;
    struct node_info* finger_table[];
    struct net_server* net;
};

int node_id_compare(hash_type id1, hash_type id2){
    if (id1 == id2) return 0;
    if (id1 < id2) return -1;
    return 1;
}

// inclusive at both ends
int node_id_in_range(hash_type id, hash_type min, hash_type max)
{
    if (max > min){ //easy
        return  ((id >= min) && (id <= max));
    }else if (max < min){ // min -> 0 -> max
        return  ((id >= min) || ((id >= 0) && (id <= max)));
    }else{ //max == min
        return (id == max);
    }
}

struct node_self* node_create()
{
    struct node_self* node = malloc(sizeof(struct node_self));
    if(!node) { return NULL; }

    node->id = 0;
    node->predecessor = {0, 0, 0};
    node->successor = {0, 0, 0};

    node->finger_table = malloc(sizeof(node_info) * ID_BITS);
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

void node_destroy(struct node_self* n)
{
    if (!n) { return; }
    if (n->net){ net_server_destroy(n->net); }
    if (n->finger_table){ free(n->finger_table); }
    free(n);
}

int node_network_create(struct node_self* self)
{
    n->predecessor = self->id;
    n->has_pred = 1;

    int rv = net_server_run(self->net);
    if (rv) return -1;
    return 0;
}

int node_network_join(struct node_self* self, node_info node)
{
    self->has_pred = 0; // nil
    self->successor = node_find_successor_remote(self, node, self->id);
    return 0;
}

// TODO this needs to call a callback instead of returning a value because it might have to wait on  net comms
struct node_info node_find_successor(struct node_self* self, hash_type id)
{
    if (node_id_compare(self->id, id) == 0){ // id is my id
            return self->self;
    }

    if(node_id_in_range(id, self->id, self->successor.id)){ // id is between me and my successor
        return self->successor;
    }else{ // need to ask another node to find it
        struct node_info n = node_closest_preceding_node(self, id);
        node_find_successor_remote(self, n, id);
    }
}

struct node_info node_find_successor_remote(struct node_self* self, struct node_info n, hash_type id)
{

}

int node_send_message(struct node_self* self, node_message* message)
{

}

struct node_info node_closest_preceding_node(struct node_self* self, hash_type id)
{
    for (hash_type i = ID_BITS-1; i >= 0; --i){
        hash_type n = self->finger_table[i]->id;
        if (node_id_in_range(n, self->id, id)){
            return (n->id);
        }
    }
    return (self->id);
}

void node_network_stabalize(struct node_self* self)
{
    // get successors predecessor
    // reply goes to callback: if (me < s->p < s) then update me->s
    //                         notify s
}

void node_notify_node(struct node_self* self, hash_type node_id)
{
    char msg[100];
    snprintf(msg, 100, "%X\n%X\n%s%s", self->id, node_id, REQ_NOTIFY, MES_END);
    node_send_message(self, msg, node_id);
}

void node_notified(struct node_self* self, struct node_info node)
{
    if (!self->has_pred || node_id_in_range(node->id, self->predecessor, self->id)){
        self->predecessor = node;
        self->has_pred = 1;
    }
}

void node_fix_fingers(struct node_self* self)
{

}

void node_check_predecessor(struct node_self* self)
{
    if (!self->has_pred){ return; }
    char msg[100];
    snprintf(msg, 100, "%X\n%X\n%s%s", self->id, self->predecessor, REQ_ALIVE, MES_END);
    node_send_message(self, msg, node_id);
}

void handle_message(struct node_self* self, struct node_message* message, struct evbuffer *replyto)
{
    struct node_info other;
    switch (msg->type){

        case MSG_T_SUCC_REQ:                                                         // chars + ID + IP + PORT (numbers in hex)
            evbuffer_add_printf(replyto, MSG_FMT, msg->to, msg->from, RESP_SUCCESSOR, (15 + (ID_BITS/4) + (32/4) + (16/4)));
            evbuffer_add_printf(replyto, "id:%X\nipv4:%X\nport:%X", self->successor->id, self->successor->IP, self->successor->port);
            break;
        case MSG_T_PRED_REQ:                                                         // chars + ID + IP + PORT (numbers in hex)
            evbuffer_add_printf(replyto, MSG_FMT, msg->to, msg->from, RESP_PREDECESSOR, (15 + (ID_BITS/4) + (32/4) + (16/4)));
            evbuffer_add_printf(replyto, "id:%X\nipv4:%X\nport:%X", self->predecessor->id, self->predecessor->IP, self->predecessor->port);
            break;
        case MSG_T_ALIVE_REQ:
            evbuffer_add_printf(replyto, MSG_FMT, msg->to, msg->from, RESP_ALIVE, 0);
            break;
        case MSG_T_NOTIF:
            other.id = message->from;
            // TODO parse content -> other
            node_notified(self, other);
            break;

        case MSG_T_SUCC_REP:
            other.id = message->from;
            // TODO parse content -> other
            break;
        case MSG_T_PRED_REP:
            other.id = message->from;
            // TODO parse content -> other
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
    struct node_self* self = (struct node_self*) arg;
    struct node_message msg;
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
    if (msg.type = parse_msg_type(token) == -1) {
            return}; //TODO close connection
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
















