#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "node.h"
#include "netio.h"
#include "proto.h"
#include "logging.h"

#ifdef USE_NETW
#include "net_wrapper.h"
#endif // USE_NETW

#define MSG_T_NOTIF     1
#define MSG_T_SUCC_REP  2
#define MSG_T_SUCC_REQ  3
#define MSG_T_PRED_REP  4
#define MSG_T_PRED_REQ  5
#define MSG_T_ALIVE_REP 6
#define MSG_T_ALIVE_REQ 7
#define MSG_T_NODE_MSG  8
#define MSG_T_UNKNOWN (-1)

const struct timeval *NODE_WAIT_TM_DEFAULT = NULL;
const struct timeval *NODE_WAIT_TM_LONG = NULL;

struct node_found_cb_data;

struct node_self{
    struct node_info self;
    struct node_info successor;
    struct node_info predecessor;
    short has_pred;
    struct node_info* finger_table;
    struct net_server* net;
    node_msg_cb_t msg_cb;
    void* msg_cb_arg;
#ifdef USE_NETW
    int netw_handle;
#endif // USE_NETW
};

struct node_join_cb_data{
    on_join_cb_t joined_cb;
    void *joined_cb_arg;
    struct node_self* self;
};

struct node_found_cb_data{
    struct node_self* self;
    node_found_cb_t cb;
    void* found_cb_arg;
    struct node_info node;
};

struct finger_update_arg{
    struct node_self* self;
    int finger_num;
};

struct incoming_handler_data{
    struct node_self* self;
    int connection;
};

void node_tm_stabilise(evutil_socket_t fd, short what, void *arg);

void node_tm_fix_fingers(evutil_socket_t fd, short what, void *arg);

void node_tm_check_pred(evutil_socket_t fd, short what, void *arg);


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
        return  ((id >= min) || (id <= max));
    }else{ //max == min
        return (id == max);
    }
}

//
// node creation and cleanup
//

void incoming_connection(int connection, short type, void *arg);

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

    #ifdef USE_NETW
    netw_init();
    node->net = netw_net_server_create(listen_port);
    netw_register_handler(incoming_connection, (void*) node);
    #else
    node->net = net_server_create(listen_port, incoming_connection, (void*) node);
    #endif // USE_NETW

    if (!node->net){
            free(node->finger_table);
            free(node);
            return NULL; }

    struct event_base* base = net_get_base(node->net);

    if (!NODE_WAIT_TM_DEFAULT){
        struct timeval default_tm = {NODE_TIMEOUT, 0};
        NODE_WAIT_TM_DEFAULT = event_base_init_common_timeout(base, &default_tm);
    }
    if (!NODE_WAIT_TM_LONG){
        struct timeval long_tm = {(NODE_TIMEOUT * 3), 0};
        NODE_WAIT_TM_LONG = event_base_init_common_timeout(base, &long_tm);
    }


    return node;
}

void node_destroy(struct node_self* n)
{
    #ifdef USE_NETW
    netw_destroy();
    #endif // USE_NETW

    if (!n) { return; }
    if (n->net){ net_server_destroy(n->net); }
    if (n->finger_table){ free(n->finger_table); }
    free(n);
}

struct net_server* node_get_net(struct node_self* self)
{
    return self->net;
}

//
// node network join/create
//


void node_set_node_msg_handler(struct node_self* self, node_msg_cb_t node_msg_cb, void* cb_arg)
{
    self->msg_cb = node_msg_cb;
    self->msg_cb_arg = cb_arg;
}


void node_network_joined(evutil_socket_t fd, short what, void *arg)
{
    log_info("network joined\n");
    struct node_join_cb_data* cb_data = (struct node_join_cb_data*) arg;
    cb_data->joined_cb(cb_data->joined_cb_arg);
    log_info("got cb data\n");

    struct node_self* self = cb_data->self;
    log_info("got self\n%X\n", self);
    struct event_base *base = net_get_base(self->net);
    log_info("got base\n");

    struct event* stabilise_tm_ev;
    struct event* fix_finger_tm_ev;
    struct event* check_pred_tm_ev;

    struct timeval stab_tm = {STABILIZE_PERIOD, 0};
    const struct timeval *stab_tm_comm = event_base_init_common_timeout(base, &stab_tm);
    log_info("got common timeval\n");

    stabilise_tm_ev  = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, node_tm_stabilise,   (void*) self);
    fix_finger_tm_ev = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, node_tm_fix_fingers, (void*) self);
    check_pred_tm_ev = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, node_tm_check_pred,  (void*) self);

    log_info("created stab evs\n");

    event_add(stabilise_tm_ev, stab_tm_comm);
    event_add(fix_finger_tm_ev, stab_tm_comm);
    event_add(check_pred_tm_ev, stab_tm_comm);
    //TODO call all stabs now?
    log_info("added stab evs\n");


    free(cb_data);
    log_info("freed cb data\n");
}

int node_network_create(struct node_self* self, on_join_cb_t join_cb, void *arg)
{
    self->predecessor = self->self;
    self->has_pred = 1;

    struct node_join_cb_data* cb_data = malloc(sizeof(struct node_join_cb_data));

    cb_data->joined_cb = join_cb;
    cb_data->joined_cb_arg = arg;
    cb_data->self = self;

    struct timeval tmo = {0,1};
    event_base_once(net_get_base(self->net), -1, EV_TIMEOUT, node_network_joined, cb_data, &tmo);

    log_info("running server...\n");
    return net_server_run(self->net);
}

void node_network_join_succ_found(struct node_info succ, void *arg)
{
    struct node_join_cb_data* cb_data = (struct node_join_cb_data*) arg;

    log_info("succ found for join\n");

    struct node_self* self = cb_data->self;
    self->successor = succ;

    struct timeval tmo = {0,1};
    event_base_once(net_get_base(self->net), -1, EV_TIMEOUT, node_network_joined, cb_data, &tmo);
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
    node_find_successor_remote(self, node, self->self.id, cb_data);
    log_info("running server...\n");
    return net_server_run(self->net);
}

//
// Timeout callbacks for stabilization
//

void node_tm_stabilise(evutil_socket_t fd, short what, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    node_network_stabalize(self);
}

void node_tm_fix_fingers(evutil_socket_t fd, short what, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    node_fix_fingers(self);
}

void node_tm_check_pred(evutil_socket_t fd, short what, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    node_check_predecessor(self);
}

//
// callbacks for when node is found
//

void node_found(evutil_socket_t fd, short what, void *arg)
{
    log_info("found node");
    struct node_found_cb_data* cb_data = (struct node_found_cb_data*) arg;
    cb_data->cb(cb_data->node, cb_data->found_cb_arg);
    free(cb_data);
}

void node_found_remote_cb(int connection, void *arg)
{
    log_info("reply from get succ remote");
    struct node_found_cb_data* cb_data = (struct node_found_cb_data*) arg;
    struct node_self* self = cb_data->self;

    struct evbuffer *read_buf = net_connection_get_read_buffer(self->net, connection);
    size_t line_len;
    char* endptr;
    char* token;

    token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);

    if (strncmp(token, "NONE", 4)){
        log_info("couldn't find it");
        cb_data->node.id = 0;
        cb_data->node.IP = 0;
        cb_data->node.port = 0;
        free(token);
    }else{
        free(token);
        token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
        endptr = NULL;
        cb_data->node.id = strtoull(token, &endptr, 16);
        free(token);

        token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
        endptr = NULL;
        cb_data->node.IP = strtoull(token, &endptr, 16);
        free(token);

        token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
        endptr = NULL;
        cb_data->node.port = strtoull(token, &endptr, 16);
        free(token);
    }

    net_connection_close(self->net, connection);

    node_found(-1, 0, cb_data);
}

void node_remote_find_event(int connection, short type, void *arg)
{

}

//
// node finding
//

//ask node n for successor of id
int node_find_successor_remote(struct node_self* self, struct node_info n,
                               hash_type id, struct node_found_cb_data* cb_data)
{
    log_info("find_succ_remote");
    struct node_message msg;
    msg.from = self->self;
    msg.to   = n;
    msg.type = MSG_T_SUCC_REQ;

    char content[(ID_BITS/4) + 1];
    snprintf(content, (ID_BITS/4) + 1, "%X", id);
    msg.len  = ID_BITS/4;
    msg.content = content;

    log_info("built msg");
    return node_connect_and_send_message(self, &msg, node_found_remote_cb,
                                         node_remote_find_event, (void*) cb_data, NODE_WAIT_TM_LONG);
}

int node_find_successor(struct node_self* self, hash_type id, node_found_cb_t cb, void* found_cb_arg)
{
    log_info("looking for successor");

    struct node_found_cb_data *cb_data;
    if (node_id_compare(self->self.id, id) == 0){ // id is my id
        log_info("it's me");

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
    if(node_id_in_range(id, self->self.id, self->successor.id)){ // id is between me and my successor
        log_info("it's my succ");

        cb_data = malloc(sizeof(struct node_found_cb_data));
        cb_data->cb           = cb;
        cb_data->found_cb_arg = found_cb_arg;
        cb_data->node         = self->successor;

        if (event_base_once(net_get_base(self->net), -1, 0, node_found, cb_data, NULL) == -1){
            free(cb_data);
            return -1;
        };

    }else{ // need to ask another node to find it
        log_info("need to ask someone else");
        struct node_info n = node_closest_preceding_node(self, id); // node to ask

        cb_data = malloc(sizeof(struct node_found_cb_data));
        cb_data->cb           = cb;
        cb_data->found_cb_arg = found_cb_arg;

        return node_find_successor_remote(self, n, id, cb_data);
    }
    return 0;
}

void node_get_predecessor_remote(struct node_self* self, struct node_info n,
                                 node_found_cb_t cb, void* found_cb_arg)
{
    struct node_found_cb_data* cb_data;
    struct node_message msg;
    msg.from = self->self;
    msg.to   = n;
    msg.type = MSG_T_PRED_REQ;

    msg.len  = 0;
    msg.content = NULL;

    cb_data = malloc(sizeof(struct node_found_cb_data));
    cb_data->cb           = cb;
    cb_data->found_cb_arg = found_cb_arg;


    log_info("asking for pred");

    node_connect_and_send_message(self, &msg, node_found_remote_cb,
                                  node_remote_find_event, (void*) cb_data, NODE_WAIT_TM_LONG);
}

struct node_info node_closest_preceding_node(struct node_self* self, hash_type id)
{
    for (hash_type i = ID_BITS-1; i > 0; --i){
        struct node_info n = self->finger_table[i];
        if (node_id_in_range(n.id, self->self.id, id)){
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
    log_info("got succ's pred for stab");

    struct node_self* self = (struct node_self*) arg;

    if (succ.port == 0 && succ.IP == 0){ // successor doesn't know its predecessor

    }else
    // if (me < s->p < s) then update me->s
    if (node_id_in_range(succ.id, self->self.id, self->successor.id)){
        self->successor = succ;
    }
    // notify s
    node_notify_node(self, self->successor);
}

void node_network_stabalize(struct node_self* self)
{
    log_info("stabilizing");
    node_get_predecessor_remote(self, self->successor, node_stabilize_sp_found, self);
}

// TODO
void node_notify_rep_event(int connection, short type, void *arg){

}

void node_notify_node(struct node_self* self, struct node_info node)
{
    struct node_message msg;
    msg.from = self->self;
    msg.to   = node;
    msg.type = MSG_T_NOTIF;

    char content[ID_HEX_CHARS + 1 + (16/4) + 1 + 1];
    snprintf(content, sizeof(content)-1, "%08X\n%04X\n", self->self.id, self->self.port);
    msg.len = ID_HEX_CHARS + 1 + (16/4) + 1;
    msg.content = content;

    node_connect_and_send_message(self, &msg, NULL, node_notify_rep_event, (void*)self, NODE_WAIT_TM_DEFAULT);
}

void node_notified(struct node_self* self, struct node_info node)
{
    log_info("got notified");
    if (!self->has_pred || node_id_in_range(node.id, self->predecessor.id, self->self.id)){
        self->predecessor = node;
        self->has_pred = 1;
    }
}

//
// Fix Fingers
//

void finger_update(struct node_info node, void *arg)
{
    struct finger_update_arg* fua = (struct finger_update_arg*) arg;

    fua->self->finger_table[fua->finger_num] = node;
    free(fua);
}

void node_fix_a_finger(struct node_self* self, int finger_num)
{
    if (finger_num >= ID_BITS || finger_num <= 0){
        return; }// non-existent finger
    // this might need redoing if hash_type changes
    hash_type finger_id = self->self.id + (hash_type) two_to_the_n(finger_num - 1);

    struct finger_update_arg* fua = malloc(sizeof(struct finger_update_arg));
    fua->finger_num = finger_num;
    fua->self = self;

    node_find_successor(self, finger_id, finger_update, fua);
}

void node_fix_fingers(struct node_self* self)
{
    log_info("fixing fingers");
    for(int fnum = 0; fnum < ID_BITS; ++fnum){
        node_fix_a_finger(self, fnum);
    }
}

void node_check_predecessor_reply(int connection, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    struct evbuffer* read_buf = net_connection_get_read_buffer(self->net, connection);

    size_t line_len;
    char* token;

    token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
    if (!strncmp(token, "ALIVE", 5)){
        // HOORAY
        log_info("pred is not dead");
    }else{
        // couldn't reach pred
        self->has_pred = 0;
        memset(&(self->predecessor), 0, sizeof(struct node_info));
        log_info("pred is dead");
    }
    free(token);
    net_connection_close(self->net, connection);
}

void node_check_predecessor_event(int connection, short type, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    if (type & (BEV_EVENT_EOF|BEV_EVENT_ERROR|BEV_EVENT_TIMEOUT)) {
        if (type & (BEV_EVENT_ERROR|BEV_EVENT_TIMEOUT)) {
            // couldn't reach pred
            self->has_pred = 0;
            memset(&(self->predecessor), 0, sizeof(struct node_info));
        }
        net_connection_close(self->net, connection);
    }
}

void node_check_predecessor(struct node_self* self)
{
    if (!self->has_pred){ return; }
    log_info("checking predecessor");

    struct node_message msg;
    msg.from = self->self;
    msg.to   = self->predecessor;
    msg.type = MSG_T_ALIVE_REQ;
    msg.len  = 0;
    msg.content = NULL;

    node_connect_and_send_message(self, &msg, node_check_predecessor_reply,
                                  node_check_predecessor_event, (void*) self, NODE_WAIT_TM_DEFAULT);
}

//
// network I/O wrapper and node communication things
//

char* node_msg_type_str(int msg_type){
    switch (msg_type){
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
        case MSG_T_NODE_MSG:
            return NODE_MSG;
        default:
            return "NONE";
    }
}

int node_send_message(struct node_self* self, struct node_message* msg, const int connection)
{
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    if (!write_buf){
        return -1; }

    log_info("got write buf");

    int rc = 0;

    if (msg->content != NULL){
        rc = evbuffer_add_printf(write_buf, MSG_FMT_CONTENT, msg->from.id, msg->to.id,
                                 node_msg_type_str(msg->type), msg->len, msg->content);
    }else{
        rc = evbuffer_add_printf(write_buf, MSG_FMT, msg->from.id, msg->to.id,
                                 node_msg_type_str(msg->type), 0);
    }

    log_info("added content to buf");

    if (rc < 0){
        return rc;
    }

    log_info("activating connection");
    return net_connection_activate(self->net, connection);
}


int node_connect_and_send_message(struct node_self* self,
                                  struct node_message* msg,
                                  net_connection_data_cb_t read_cb,
                                  net_connection_event_cb_t event_cb,
                                  void *cb_arg,
                                  const struct timeval *timeout)
{
    #ifdef USE_NETW
    int connection = netw_net_connection_create(self->net, msg->to.IP, msg->to.port, self->netw_handle);
    #else
    int connection = net_connection_create(self->net, msg->to.IP, msg->to.port);
    #endif // USE_NETW

    log_info("got connection %d", connection);
    if(connection < 0){
        // error creating connection
        return connection;
    }
    if (timeout){
        net_connection_set_timeouts(self->net, connection, timeout, timeout);
    }
    if (read_cb){
        net_connection_set_read_cb(self->net, connection, read_cb);
    }
    if (event_cb){
        net_connection_set_event_cb(self->net, connection, event_cb);
    }
    if (cb_arg){
        net_connection_set_cb_arg(self->net, connection, cb_arg);
    }

    log_info("calling send msg");
    return node_send_message(self, msg, connection);
}


void node_successor_found_for_remote(struct node_info succ, void *data)
{
    log_info("successor foud for remote at %08X:%d", succ.IP, succ.port);
    struct incoming_handler_data* handler_data = (struct incoming_handler_data*) data;
    struct evbuffer* write_buf = net_connection_get_write_buffer(
                                 handler_data->self->net, handler_data->connection);

    evbuffer_add_printf(write_buf, "%X\n%X\n%X\n", succ.id, succ.IP, succ.port);

    free(handler_data);
}

void handle_succ_request(struct node_self* self, int connection, hash_type r_id)
{
    log_info("handling succ req");
    struct incoming_handler_data *handler_data;
    handler_data = malloc(sizeof(struct incoming_handler_data));
    handler_data->self = self;
    handler_data->connection = connection;
    node_find_successor(self, r_id, node_successor_found_for_remote, handler_data);
}

void handle_alive_request(struct node_self* self, int connection)
{
    log_info("alive req recvd");
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    evbuffer_add(write_buf, "ALIVE\n", 6);
}

void handle_pred_request(struct node_self* self, int connection)
{
    log_info("handling pred req");
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    if (self->has_pred){
        evbuffer_add_printf(write_buf, "OKAY\n%X\n%X\n%X\n",
                            self->predecessor.id, self->predecessor.IP, self->predecessor.port);
    }else{
        evbuffer_add(write_buf, "NONE\n", 5);
    }
}



void handle_succ_request(int connection, void *arg)
{
    log_info("handling succ req");
    struct incoming_handler_data *handler_data;
    handler_data = malloc(sizeof(struct incoming_handler_data));
    handler_data->self = self;
    handler_data->connection = connection;
    node_find_successor(self, r_id, node_successor_found_for_remote, handler_data);
}

void handle_pred_request(int connection, void *arg)
{
    log_info("handling pred req");
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    if (self->has_pred){
        evbuffer_add_printf(write_buf, "OKAY\n%X\n%X\n%X\n",
                            self->predecessor.id, self->predecessor.IP, self->predecessor.port);
    }else{
        evbuffer_add(write_buf, "NONE\n", 5);
    }
}

void handle_alive_request(int connection, void *arg)
{
    log_info("handling alive req");
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    evbuffer_add(write_buf, "ALIVE\n", 6);
}

handle_notif_request(int connection, void *arg)
{
    log_info("handling notif req");
    struct evbuffer* read_buf = net_connection_get_read_buffer(self->net, connection);

    token = evbuffer_readln(read_buf, &read_len, EVBUFFER_EOL_LF);
    endptr = NULL;
    other.id = strtoull(token, &endptr, 16);
    free(token);

    token = evbuffer_readln(read_buf, &read_len, EVBUFFER_EOL_LF);
    endptr = NULL;
    other.port = strtoull(token, &endptr, 16);
    free(token);

    other.IP = net_connection_get_remote_address(self->net, connection);
    node_notified(self, other);
    net_connection_close(self->net, connection);
}

void handle_message(struct node_self* self, struct node_message* msg, int connection)
{

    log_info("handling incoming msg");

    // for parsing msg body
    struct evbuffer* read_buf = net_connection_get_read_buffer(self->net, connection);
    char* endptr;
    char* saveptr;
    char* token;
    size_t read_len;
    hash_type r_id;
    struct node_info other;

    log_info("switching %d ", msg->type);
    switch (msg->type){

        case MSG_T_SUCC_REQ:
            log_info("case MSG_T_SUCC_REQ");
            token = evbuffer_readln(read_buf, &read_len, EVBUFFER_EOL_LF);
            log_info("read line");
            endptr = NULL;
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
            log_info("handling notif req");
            token = evbuffer_readln(read_buf, &read_len, EVBUFFER_EOL_LF);
            endptr = NULL;
            other.id = strtoull(token, &endptr, 16);
            free(token);

            token = evbuffer_readln(read_buf, &read_len, EVBUFFER_EOL_LF);
            endptr = NULL;
            other.port = strtoull(token, &endptr, 16);
            free(token);

            other.IP = net_connection_get_remote_address(self->net, connection);
            node_notified(self, other);
            net_connection_close(self->net, connection);
            break;

        case MSG_T_NODE_MSG:
            log_info("handing msg to node");
            self->msg_cb(self, msg, connection, self->msg_cb_arg);
            break;




        case MSG_T_SUCC_REP:
            break;
        case MSG_T_PRED_REP:
            break;
        case MSG_T_ALIVE_REP:
            break;
    }
}

int node_parse_message_header(struct node_message* msg, struct evbuffer* read_buf)
{
    char* endptr;
    char lenstr[LEN_STR_BYTES + 1];

    // type of message
    if (evbuffer_remove(read_buf, &(msg->type), 1) == -1)
    {
        // error reading
        return -1;
    }

    // length of content or 0 if none
    token = evbuffer_remove(read_buf, lenstr, LEN_STR_BYTES);
    lenstr[LEN_STR_BYTES] = '\0';
    msg->len = (uint32_t) strtoul(lenstr, &endptr, 16);
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
    log_info("incomming read cb");
    struct node_self* self = (struct node_self*) arg;

    struct node_message msg;
    struct evbuffer* read_buf = net_connection_get_read_buffer(self->net, connection);

    if (node_parse_message_header(&msg, read_buf) < 0){
        // error parsing msg
        net_connection_close(self->net, connection);
        return;
    }
    switch (msg->type){

        case MSG_T_SUCC_REQ:
            net_connection_set_read_cb(self->net, connection, handle_succ_request);
            break;

        case MSG_T_PRED_REQ:
            net_connection_set_read_cb(self->net, connection, handle_pred_request);
            break;

        case MSG_T_ALIVE_REQ:
            net_connection_set_read_cb(self->net, connection, handle_alive_request);
            break;

        case MSG_T_NOTIF:
            net_connection_set_read_cb(self->net, connection, handle_notif_request);
            break;

        case MSG_T_NODE_MSG:
            net_connection_set_read_cb(self->net, connection, handle_node_message);
            break;

        case MSG_T_SUCC_REP:
        case MSG_T_PRED_REP:
        case MSG_T_ALIVE_REP:
            break;
    }

    struct bufferevent* bufev = net_connection_get_bufev(self->net, connection);
    bufferevent_setwatermark(bufev, EV_READ, msg.len, 0);

    handle_message(self, &msg, connection);
}

void incoming_connection(int connection, short type, void *arg)
{

    log_info("incoming connection cb");
    struct node_self* self = (struct node_self*) arg;
    net_connection_set_read_cb(self->net, connection, incoming_read_cb);
    net_connection_set_event_cb(self->net, connection, incoming_event_cb);
    net_connection_set_cb_arg(self->net, connection, (void*)self);
    net_connection_set_timeouts(self->net, connection, NODE_WAIT_TM_DEFAULT, NODE_WAIT_TM_DEFAULT);
    struct bufferevent* bufev = net_connection_get_bufev(self->net, connection);
    bufferevent_setwatermark(bufev, EV_READ, 1 + LEN_STR_BYTES, 0);
}
