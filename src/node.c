#include "node.h"
#include "netio.h"
#include "proto.h"

#include <string.h>
#include <stdlib.h>

#define MSG_T_NOTIF     1
#define MSG_T_SUCC_REP  2
#define MSG_T_SUCC_REQ  3
#define MSG_T_PRED_REP  4
#define MSG_T_PRED_REQ  5
#define MSG_T_ALIVE_REP 6
#define MSG_T_ALIVE_REQ 7

struct node_self{
    hash_type id;
    struct node_info self;
    struct node_info successor;
    struct node_info predecessor;
    short has_pred;
    struct node_info* finger_table;
    struct net_server* net;
};

//
// ID helpers
//

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

//
// node creation and cleanup
//

struct node_self* node_create(int listen_port)
{
    struct node_self* node = malloc(sizeof(struct node_self));
    if(!node) { return NULL; }

    node->id = 0;
    node->predecessor = {0, 0, 0};
    node->successor = {0, 0, 0};

    node->finger_table = malloc(sizeof(struct node_info) * ID_BITS);
    if (!node->finger_table){
            free(node);
            return NULL; }

    node->net = net_server_create(listen_port, recv_message, (void*) node);
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

//
// node network join/create
//

int node_network_create(struct node_self* self)
{
    self->predecessor = self->id;
    self->has_pred = 1;

    int rv = net_server_run(self->net);
    if (rv) return -1;
    return 0;
}

int node_network_join(struct node_self* self, struct node_info node)
{
    self->has_pred = 0; // nil
    self->successor = node_find_successor_remote(self, node, self->id);
    return 0;
}

//
// callbacks for when node is found
//

struct node_found_cb_data{
    node_found_cb_t cb;
    void* found_cb_arg;
    struct node_info node;
};

void node_found(evutil_socket_t fd, short what, void *arg)
{
    struct node_found_cb_data* cb_data = (struct node_found_cb_data*) arg;
    cb_data->cb(cb_data->node, cb_data->found_cb_arg);
    free(cb_data);
}

void node_found_remote_cb(struct bufferevent *bev, void *arg)
{
    struct node_found_cb_data* cb_data = (struct node_found_cb_data*) arg;

    struct node_message msg;
    struct evbuffer *input = bufferevent_get_input(bev);
    struct evbuffer *output = bufferevent_get_output(bev);
    int header_len = 0;
    int line_len;
    char* endptr;
    char* saveptr;
    char* token;

    //TODO parse msg properly

    token = evbuffer_readln(input, &line_len, EVBUFFER_EOL_LF);
    msg.from.id = strtoull(token, &endptr, 16);
    header_len += line_len + 1;
    free(token);

    cb_data->node = ...

    bufferevent_free(bev);

    node_found(-1, 0, cb_data)
}

//
// node finding
//

//ask node n for successor of id
int node_find_successor_remote(struct node_self* self, struct node_info n, hash_type id, struct node_found_cb_data* cb_data)
{
    struct node_message msg;
    msg.from = self->self;
    msg.to   = n;
    msg.type = MSG_T_SUCC_REQ;

    char content[(ID_BITS/4) + 1];
    snprintf(content, (ID_BITS/4) + 1, "%X", id);
    msg.len  = ID_BITS/4;
    msg.content = content;

    return node_send_message(self, &msg, node_found_remote_cb, (void*) cb_data);
}

int node_find_successor(struct node_self* self, hash_type id, node_found_cb_t cb, void* found_cb_arg)
{
    struct node_found_cb_data *cb_data;
    int rv = -1;
    if (node_id_compare(self->id, id) == 0){ // id is my id

            cb_data = malloc(sizeof(struct node_found_cb_data));
            cb_data->cb           = cb;
            cb_data->found_cb_arg = found_cb_arg;
            cb_data->node         = self->self;

            if (event_base_once(net_get_base(self->net), -1, 0, node_found, cb_data, NULL) == -1){
                free(cb_data);
                return -1;
            };
    }
    else
    if(node_id_in_range(id, self->id, self->successor.id)){ // id is between me and my successor

            cb_data = malloc(sizeof(struct node_found_cb_data));
            cb_data->cb           = cb
            cb_data->found_cb_arg = found_cb_arg;
            cb_data->node         = self->successor;

            if (event_base_once(net_get_base(self->net), -1, 0, node_found, cb_data, NULL) == -1){
                free(cb_data);
                return -1;
            };

    }else{ // need to ask another node to find it
        struct node_info n = node_closest_preceding_node(self, id); // node to ask

        cb_data = malloc(sizeof(struct node_found_cb_data));
        cb_data->cb           = cb
        cb_data->found_cb_arg = found_cb_arg;

        return node_find_successor_remote(self, n, id, cb_data);
    }
    return 0;
}

void node_get_predecessor_remote(struct node_self* self, struct node_info* n, node_found_cb_t cb, void* found_cb_arg)
{
    struct node_message msg;
    msg.from = self->self;
    msg.to   = n;
    msg.type = MSG_T_PRED_REQ;

    char content[(ID_BITS/4) + 1];
    snprintf(content, (ID_BITS/4) + 1, "%X", id);
    msg.len  = ID_BITS/4;
    msg.content = content;

    cb_data = malloc(sizeof(struct node_found_cb_data));
    cb_data->cb           = cb
    cb_data->found_cb_arg = found_cb_arg;

    node_send_message(self, &msg, node_found_remote_cb, (void*) cb_data);
}

struct node_info node_closest_preceding_node(struct node_self* self, hash_type id)
{
    for (hash_type i = ID_BITS-1; i >= 0; --i){
        struct node_info n = self->finger_table[i];
        if (node_id_in_range(n->id, self->id, id)){
            return (n);
        }
    }
    return (self->self);
}

//
// network Stabilization
//

//found successor's predecessor (stabilize part 2)
void node_stabilize_sp_found(struct node_info succ, void *arg){

    struct node_self* self = (struct node_self*) arg;

    if (succ.port == 0 && succ.IP == 0){ // successor doesn't know its predecessor

    }else
    // if (me < s->p < s) then update me->s
    if (node_id_in_range(succ.id, self->id, self->successor.id)){
        self->successor = succ;
    }
    // notify s
    node_notify_node(self, self->successor);
}

void node_network_stabalize(struct node_self* self)
{
    node_get_predecessor_remote(self, self->successor, node_stabilize_sp_found, self);
}
// TODO
void node_notify_node(struct node_self* self, struct node_info* node)
{
    struct node_message msg;
    msg.from = self->self;
    msg.to   = node;
    msg.type = MSG_T_NOTIF;

    char content[_];
    snprintf(content, _, "", );
    msg.len  = ;
    msg.content = content;

    // TODO callback to close & free connection
    node_send_message(self, msg, null, null);
}

void node_notified(struct node_self* self, struct node_info node)
{
    if (!self->has_pred || node_id_in_range(node->id, self->predecessor, self->id)){
        self->predecessor = node;
        self->has_pred = 1;
    }
}

//
// Fix Fingers
//

struct finger_update_arg{
    struct node_self* self;
    int finger_num;
};

void finger_update(struct node_info node, void *data)
{
    struct finger_update_arg* fua = (struct finger_update_arg*) data;

    fua->self->finger_table[fua->finger_num] = node;
    free(fua);
}

void node_fix_a_finger(struct node_self* self, int finger_num)
{
    if (finger_num >= ID_BITS || finger_num < 0){
        return; }// non-existent finger

    hash_type finger_id = self->id + (hash_type) two_to_the_n(finger_num - 1); // this might need redoing if hash_type changes

    struct finger_update_arg* fua = malloc(sizeof(struct finger_update_arg));
    fua->finger_num = finger_num;
    fua->self = self;

    node_find_successor(self, finger_id, finger_update, fua);
}

void node_fix_fingers(struct node_self* self)
{
    for(int fnum = 0; fnum < ID_BITS; ++fnum){
        node_fix_a_finger(self, fnum);
    }
}

// TODO + add cb
void node_check_predecessor(struct node_self* self)
{
    if (!self->has_pred){ return; }

    struct node_message msg;
    msg.from = self->self;
    msg.to   = node;
    msg.type = MSG_T_ALIVE_REQ;

    char content[_];
    snprintf(content, _, "", );
    msg.len  = ;
    msg.content = content;

    node_send_message(self, msg, , );
}

//
// network I/O wrapper and node communication things
//

char* node_msg_type_str(int msg_type){
    switch (type){
        case MSG_T_ALIVE_REQ:
            return REQ_ALIVE;
        case MSG_T_NOTIF:
            return REQ_NOTIFY;
        case MSG_T_PRED_REQ:
            return REQ_PREDECESSOR;
        case MSG_T_SUCC_REQ:
            return REQ_SUCCESSOR;
        case MSG_T_ALIVE_REP:
            return RESP_ALIVE;
        case MSG_T_PRED_REP:
            return RESP_PREDECESSOR;
        case MSG_T_SUCC_REP:
            return RESP_SUCCESSOR;
    }
}

int node_send_message(struct node_self* self, struct node_message* msg, bufferevent_data_cb reply_handler, void* rh_arg)
{
    char* msg_bytes = malloc(sizeof(char) * (48 + msg->len));
    if (msg->content != NULL){
        snprintf(msg_bytes, 48, MSG_FMT_CONTENT, msg->from, msg->to, node_msg_type_str(msg->type), msg->len, msg->content);
    }else{
        snprintf(msg_bytes, 48, MSG_FMT, msg->from, msg->to, node_msg_type_str(msg->type), 0);
    }
    net_send_message(self->net, msg_bytes, , reply_handler, rh_arg); //TODO
}

struct incoming_handler_data{
    struct node_self* self;
    struct evbuffer* replyto;
    struct node_message* recvd_msg;
};

void node_successor_found_for_remote(struct node_info succ, void *data)
{
    struct incoming_handler_data* handler_data = (struct incoming_handler_data*) data;

    evbuffer_add_printf(handler_data->replyto, "id:%X\nipv4:%X\nport:%X", succ.id, succ.IP, succ.port);

    free(handler_data->recvd_msg);
    // TODO close connection and free buffer event at some point
    free(handler_data);
}

// TODO
void handle_message(struct node_self* self, struct node_message* message, struct evbuffer *replyto)
{
    struct incoming_handler_data *handler_data;
    struct node_info other;
    switch (msg->type){

        case MSG_T_SUCC_REQ:
            handler_data = malloc(sizeof(struct incoming_handler_data));
            handler_data->self = self;
            handler_data->recvd_msg = message;
            handler_data->replyto = replyto;
            // TODO get id
            node_find_successor(self, id, node_successor_found_for_remote, handler_data);
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
// TODO
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
// TODO
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
            return; } //TODO close connection
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
        if (copied < msg.len){
            // TODO not enough data
        }
    }else{
        msg.content = NULL;
    }

    handle_message(self, &msg, output);
}
















