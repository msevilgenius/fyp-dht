#include "node.h"
#include "netio.h"
#include "proto.h"
#include "libdht.h"

#include <string.h>
#include <stdlib.h>

#define MSG_T_NOTIF     1
#define MSG_T_SUCC_REP  2
#define MSG_T_SUCC_REQ  3
#define MSG_T_PRED_REP  4
#define MSG_T_PRED_REQ  5
#define MSG_T_ALIVE_REP 6
#define MSG_T_ALIVE_REQ 7


struct node_found_cb_data;

struct node_self{
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

struct node_self* node_create(uint16_t listen_port, char* name)
{
    struct node_self* node = malloc(sizeof(struct node_self));
    if(!node) { return NULL; }

    node->self.id = get_id(name);
    node->self.port = listen_port;
    node->self.IP = 0x7F000001; // (127.0.0.1)
    memset(&(node->predecessor), 0, sizeof(struct node_info));
    memset(&(node->successor), 0, sizeof(struct node_info));

    node->finger_table = malloc(sizeof(struct node_info) * ID_BITS);
    if (!node->finger_table){
            free(node);
            return NULL; }

    node->net = net_server_create(listen_port, incoming_connection, (void*) node);
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

typedef void (*on_join_cb_t)(void* arg)

struct node_join_cb_data{
    joined_cb
    void *joined_cb_arg
    struct node_self* self
};

void node_network_joined(evutil_socket_t fd, short what, void *arg)
{
    struct node_join_cb_data* cb_data = (struct node_join_cb_data*) arg;
    cb_data->joined_cb(cb_data->joined_cb_arg);

    //TODO setup stabilisation timeout cbs
    struct node_self* self = cb_data->self;


    free(cb_data);
}

int node_network_create(struct node_self* self, on_join_cb_t join_cb, void *arg)
{
    self->predecessor = self->self->id;
    self->has_pred = 1;

    struct node_join_cb_data* cb_data = malloc(sizeof(struct node_join_cb_data));

    cb_data->joined_cb = join_cb;
    cb_data->joined_cb_arg = cb_arg;

    event_base_once(net_get_base(self->net), -1, 0, node_network_joined, cb_data, NULL);

    return net_server_run(self->net);
}


void node_network_join_succ_found(struct node_info succ, void *arg);
{
    self->successor = succ;
    struct node_join_cb_data* cb_data = (struct node_join_cb_data*) arg;

    event_base_once(net_get_base(self->net), -1, 0, node_network_joined, cb_data, NULL);
}


int node_network_join(struct node_self* self, struct node_info node, on_join_cb_t join_cb, void * cb_arg)
{
    struct node_found_cb_data* cb_data = malloc(sizeof(struct node_found_cb_data));
    struct node_join_cb_data* cb_cb_data = malloc(sizeof(struct node_join_cb_data));

    cb_cb_data->joined_cb = join_cb;
    cb_cb_data->joined_cb_arg = cb_arg;

    cb_data->self = self;
    cb_data->found_cb_arg = cb_cb_data;
    cb_data->cb = node_network_join_succ_found;
    self->has_pred = 0; // nil
    node_find_successor_remote(struct node_self* self, node, self->self->id, cb_data)
    return net_server_run(self->net);
}

//
// callbacks for when node is found
//

struct node_found_cb_data{
    struct node_self* self;
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

void node_found_remote_cb(int connection, void *arg);
{
    struct node_found_cb_data* cb_data = (struct node_found_cb_data*) arg;
    struct node_self* self = cb_data->self;

    struct node_message msg;
    struct evbuffer *read_buf = net_connection_get_read_buffer(self->net, connection);
    int header_len = 0;
    int line_len;
    char* endptr;
    char* token;

    token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
    cb_data->node.id = strtoull(token, &endptr, 16);
    free(token);

    token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
    cb_data->node.port = strtoull(token, &endptr, 16);
    free(token);

    cb_data->node.IP = net_connection_get_remote_address(aeld->net, connection);

    net_connection_close(self->srv, connection);

    node_found(-1, 0, cb_data)
}

node_remote_find_event(int connection, short type, void *arg)
{

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
    return node_connect_and_send_message(self, &msg, node_found_remote_cb, node_remote_find_event, (void*) cb_data, NODE_TIMEOUT * 3);
}

int node_find_successor(struct node_self* self, hash_type id, node_found_cb_t cb, void* found_cb_arg)
{
    struct node_found_cb_data *cb_data;
    int rv = -1;
    if (node_id_compare(self->self->id, id) == 0){ // id is my id

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
    if(node_id_in_range(id, self->self->id, self->successor.id)){ // id is between me and my successor

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
        if (node_id_in_range(n->id, self->self->id, id)){
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
    if (node_id_in_range(succ.id, self->self->id, self->successor.id)){
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
void node_notify_rep_event(int connection, short type, void *arg){

}

void node_notify_node(struct node_self* self, struct node_info* node)
{
    struct node_message msg;
    msg.from = self->self;
    msg.to   = node;
    msg.type = MSG_T_NOTIF;

    char content[ID_HEX_CHARS + 1 + (16/4) + 1 + 1];
    snprintf(content, "%08X\n%04X\n", self->self->id, self->self->port);
    msg.len = ID_HEX_CHARS + 1 + (16/4) + 1;
    msg.content = content;

    node_connect_and_send_message(self, &msg, null, node_notify_rep_event, (void*)self, NODE_TIMEOUT)
}

void node_notified(struct node_self* self, struct node_info node)
{
    if (!self->has_pred || node_id_in_range(node->id, self->predecessor, self->self->id)){
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

void finger_update(struct node_info node, void *arg)
{
    struct finger_update_arg* fua = (struct finger_update_arg*) arg;

    fua->self->finger_table[fua->finger_num] = node;
    free(fua);
}

void node_fix_a_finger(struct node_self* self, int finger_num)
{
    if (finger_num >= ID_BITS || finger_num < 0){
        return; }// non-existent finger

    hash_type finger_id = self->self->id + (hash_type) two_to_the_n(finger_num - 1); // this might need redoing if hash_type changes

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

void node_check_predecessor_reply(int connection, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    // TODO check message is legit?
    // struct evbuffer* rbuff = net_connection_get_read_buffer(self->net, connection);

    net_connection_close(self->net, connection);
}

void node_check_predecessor_event(int connection, short type, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    if (events & (BEV_EVENT_EOF|BEV_EVENT_ERROR|BEV_EVENT_TIMEOUT)) {
        if (events & (BEV_EVENT_ERROR|BEV_EVENT_TIMEOUT)) {
            // couldn't reach pred
            self->has_pred = 0;
            memset(&(node->predecessor), 0, sizeof(struct node_info));
        }
        net_connection_close(self->net, connection);
    }
}

void node_check_predecessor(struct node_self* self)
{
    if (!self->has_pred){ return; }

    struct node_message msg;
    msg.from = self->self;
    msg.to   = node;
    msg.type = MSG_T_ALIVE_REQ;
    msg.len  = 0;
    msg.content = NULL;

    node_connect_and_send_message(self, &msg, node_check_predecessor_reply, node_check_predecessor_event, (void*) self, NODE_TIMEOUT);
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

int node_send_message(struct node_self* self, struct node_message* msg, const int connection)
{
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    if (!write_buf){
        return -1; }
    int rc = 0;

    if (msg->content != NULL){
        rc = evbuffer_add_printf(write_buf, MSG_FMT_CONTENT, msg->from, msg->to, node_msg_type_str(msg->type), msg->len, msg->content);
    }else{
        rc = evbuffer_add_printf(write_buf, MSG_FMT, msg->from, msg->to, node_msg_type_str(msg->type), 0);
    }

    if (rc < 0){
        return rc
    }

    return net_connection_activate(self->net, connection);
}


int node_connect_and_send_message(struct node_self* self, struct node_message* msg, net_connection_data_cb_t read_cb, net_connection_event_cb_t event_cb, void *cb_arg, time_t timeout_secs)
{
    int connection = net_connection_create(self->net, msg->to.IP, msg->to.port);
    if(connection < 0){
        // error creating connection
        return connection;
    }
    if (timeout_secs >= 0){
        net_connection_set_timeouts(self->net, connection, timeout_secs, timeout_secs);
    }
    if (read_cb){
        net_connection_set_read_cb(self->net, connection, read_cb);
    }
    if (event_cb){
        net_connection_set_event_cb(self->net, connection, event_cb);
    }
    if (read_cb){
        net_connection_set_cb_arg(self->net, connection, cb_arg);
    }

    return node_send_message(self, msg, connection);
}

struct incoming_handler_data{
    struct node_self* self;
    int connection;
};

void node_successor_found_for_remote(struct node_info succ, void *data)
{
    struct incoming_handler_data* handler_data = (struct incoming_handler_data*) data;
    struct evbuffer* write_buf = net_connection_get_write_buffer(handler_data->self->net, handler_data->connection);

    evbuffer_add_printf(write_buf, "%X\n%X\n%X\n", succ.id, succ.IP, succ.port);

    free(handler_data);
}

void handle_succ_request(struct node_self* self, int connection, hash_type r_id)
{
    struct incoming_handler_data *handler_data;
    handler_data = malloc(sizeof(struct incoming_handler_data));
    handler_data->self = self;
    handler_data->connection = connection;
    node_find_successor(self, r_id, node_successor_found_for_remote, handler_data);
}

void handle_pred_request(struct node_self* self, int connection)
{
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    if (self->has_pred){
        evbuffer_add_printf(write_buf, "OK\n%X\n%X\n%X\n", self->predecessor.id, self->predecessor.IP, self->predecessor.port);
    }else{
        evbuffer_add(write_buf, "NONE\n", 5);
    }
}

void handle_alive_request(struct node_self* self, int connection)
{
    struct evbuffer* write_buf = net_connection_get_write_buffer(elf->net, connection);
    evbuffer_add(write_buf, "ALIVE\n", 6);
}

// TODO
void handle_message(struct node_self* self, struct node_message* message, int connection)
{

    // for parsing msg body
    struct evbuffer* read_buf = net_connection_get_read_buffer(self->net, connection);
    char* endptr;
    char* saveptr;
    char* token;
    int read_len
    hash_type r_id;
    struct node_info other;

    switch (msg->type){

        case MSG_T_SUCC_REQ:
            token = evbuffer_readln(read_buf, &read_len, EVBUFFER_EOL_LF);
            r_id = strtoull(token, &endptr, 16);
            free(token);
            handle_succ_request(self, connection, r_id);
            break;

        case MSG_T_PRED_REQ:
            handle_pred_request(self, connection);
            break;

        case MSG_T_ALIVE_REQ:
            handle_alive_request(self, connection);
            break;

        case MSG_T_NOTIF:
            other.id = message->from;
            // TODO
            net_connection_get_remote_address
            node_notified(self, other);
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

int node_parse_message_header(struct node_message* msg, struct evbuffer* read_buf)
{
    int header_len = 0;
    int line_len;

    char* endptr;
    char* saveptr;
    char* token;

    // from id
    token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
    msg.from.id = strtoull(token, &endptr, 16);
    header_len += line_len + 1;
    free(token);

    // to id
    token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
    msg.to.id = strtoull(token, &endptr, 16);
    header_len += line_len + 1;
    free(token);

    // type of message
    token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
    if (msg.type = parse_msg_type(token) == -1) {
            return -1; }
    header_len += line_len + 1;
    free(token);

    // length of content or 0 if none
    token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
    msg.len = (int) strtoul(token, &endptr, 10);
    header_len += line_len + 1;
    free(token);
    return 0;
}

void incoming_event_cb(int connection, short type, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    if (type & (BEV_ERROR|BEV_EVENT_EOF)){
        net_connection_close(self->net, connection);
    }
}


void incoming_read_cb(int connection, void *arg)
{
    struct node_self* self = (struct node_self*) arg;

    struct node_message msg;
    struct evbuffer* read_buf = net_connection_get_read_buffer(self->net, connection);

    if (node_parse_message_header(&msg, read_buf) < 0){
        // error parsing msg
        net_connection_close(self->net, connection);
        return;
    }

    if (msg.len > 0){
        msg.content = malloc(sizeof(char)*msg.len);
        int copied = evbuffer_remove(read_buf, msg.content, msg.len);
        if (copied == -1) {
            // TODO error reading
        }
        if (copied < msg.len){
            // TODO not enough data
        }
    }else{
        msg.content = NULL;
    }

    handle_message(self, &msg, connection);
}

void incoming_connection(int connection, short type, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    net_connection_set_read_cb(self->net, connection, incoming_read_cb);
    net_connection_set_event_cb(self->net, connection, incoming_event_cb);
    net_connection_set_cb_arg(self->net, connection, (void*)self);
    net_connection_set_timeouts(self->net, connection, 20, 20);
}

















