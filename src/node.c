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


const struct timeval *NODE_WAIT_TM_DEFAULT = NULL;
const struct timeval *NODE_WAIT_TM_LONG = NULL;

struct node_found_cb_data;

struct node_self{
    struct node_info self;
    struct node_info successor[NUM_OF_SUCCS];
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
    struct event* evt;
};

struct node_found_cb_data{
    struct node_self* self;
    node_found_cb_t cb;
    void* found_cb_arg;
    struct node_info node;
    struct event* evt;
};

struct finger_update_arg{
    struct node_self* self;
    int finger_num;
};

struct incoming_handler_data{
    struct node_self* self;
    int connection;
};

struct node_msg_arg{
    struct node_self* self;
    struct node_message msg;
};

struct check_succ_arg{
    struct node_self* self;
    int succ;
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

    if(!node) {
        log_err("failed to malloc node");
        return NULL;
    }

    node->self.id = get_id(name);
    node->self.port = listen_port;
    node->self.IP = 0x7F000001; // (127.0.0.1)
    memset(&(node->predecessor), 0, sizeof(struct node_info));
    memset(&(node->successor[0]), 0, sizeof(struct node_info) * NUM_OF_SUCCS);

    node->finger_table = malloc(sizeof(struct node_info) * ID_BITS);
    if (!node->finger_table){
        log_err("failed to malloc finger table");
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
        log_err("failed to create net");
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
    //log_info("network joined\n");
    struct node_join_cb_data* cb_data = (struct node_join_cb_data*) arg;
    cb_data->joined_cb(cb_data->joined_cb_arg);
    //log_info("got cb data\n");

    struct node_self* self = cb_data->self;
    //log_info("got self\n%X\n", self); // TODO remove
    struct event_base *base = net_get_base(self->net);
    //log_info("got base\n");

    struct event* stabilise_tm_ev;
    struct event* fix_finger_tm_ev;
    struct event* check_pred_tm_ev;

    struct timeval stab_tm = {STABILIZE_PERIOD, 0};
    const struct timeval *stab_tm_comm = event_base_init_common_timeout(base, &stab_tm);
    //log_info("got common timeval\n");

    stabilise_tm_ev  = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, node_tm_stabilise,   (void*) self);
    fix_finger_tm_ev = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, node_tm_fix_fingers, (void*) self);
    check_pred_tm_ev = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, node_tm_check_pred,  (void*) self);

    //log_info("created stab evs\n");

    event_add(stabilise_tm_ev, stab_tm_comm);
    event_add(fix_finger_tm_ev, stab_tm_comm);
    event_add(check_pred_tm_ev, stab_tm_comm);
    //TODO call stab now?
    //log_info("added stab evs\n");

    node_tm_stabilise(0,0,(void*)self);
    node_tm_fix_fingers(0,0,(void*)self);


    free(cb_data);
    //log_info("freed cb data\n");
}

int node_network_create(struct node_self* self, on_join_cb_t join_cb, void *arg)
{
    self->successor[0] = self->self;
    self->predecessor = self->self;
    self->has_pred = 1;

    struct node_join_cb_data* cb_data = malloc(sizeof(struct node_join_cb_data));

    cb_data->joined_cb = join_cb;
    cb_data->joined_cb_arg = arg;
    cb_data->self = self;

    struct timeval tmo = {0,1};
    struct event* crtevt;
    crtevt = event_new(net_get_base(self->net), -1, 0, node_network_joined, cb_data);
    if (!crtevt){
        log_err("failed to create event");
        free(cb_data);
        return -1;
    }else{
        cb_data->evt = crtevt;
    }
    event_active(crtevt, 0, 0);

    //log_info("running server...\n");
    return net_server_run(self->net);
}

void node_network_join_succ_found(struct node_info succ, void *arg)
{
    struct node_join_cb_data* cb_data = (struct node_join_cb_data*) arg;

    //log_info("succ found for join\n");

    struct node_self* self = cb_data->self;
    self->successor[0] = succ;

    struct event* crtevt;
    crtevt = event_new(net_get_base(self->net), -1, 0, node_network_joined, cb_data);
    if (!crtevt){
        log_err("failed to create event");
        free(cb_data);
    }else{
        cb_data->evt = crtevt;
        event_active(crtevt, 0, 0);
    }
}

int node_network_join(struct node_self* self, struct node_info node, on_join_cb_t join_cb, void * cb_arg)
{
    // TODO check malloc worked
    struct node_found_cb_data* cb_data = malloc(sizeof(struct node_found_cb_data));
    struct node_join_cb_data* cb_cb_data = malloc(sizeof(struct node_join_cb_data));

    cb_cb_data->joined_cb = join_cb;
    cb_cb_data->joined_cb_arg = cb_arg;
    cb_cb_data->self = self;

    cb_data->self = self;
    cb_data->found_cb_arg = cb_cb_data;
    cb_data->cb = node_network_join_succ_found;
    self->has_pred = 0; // nil
    node_find_successor_remote(self, node, self->self.id, cb_data);
    //log_info("running server...\n");
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

void node_tm_check_succs(evutil_socket_t fd, short what, void *arg)
{
    struct node_self* self = (struct node_self*) arg;
    node_check_successors(self);
}

//
// callbacks for when node is found
//

void node_found(evutil_socket_t fd, short what, void *arg)
{
    struct node_found_cb_data* cb_data = (struct node_found_cb_data*) arg;
    ///log_info("found node %08X @ %08X:%d", cb_data->node.id, cb_data->node.IP, cb_data->node.port);
    cb_data->cb(cb_data->node, cb_data->found_cb_arg);
    free(cb_data);
}

//TODO split into two parts one for header one for body so we can set watermarks
void node_found_remote_cb(int connection, void *arg)
{
    //log_info("reply from get succ remote");
    struct node_found_cb_data* cb_data = (struct node_found_cb_data*) arg;
    struct node_self* self = cb_data->self;

    int rc = 0;
    struct evbuffer *read_buf = net_connection_get_read_buffer(self->net, connection);

    char result[1];

    rc = evbuffer_remove(read_buf, result, 1);

    if (result[0] != 'Y'){
        log_warn("couldn't find it");
        cb_data->node.id = 0;
        cb_data->node.IP = 0;
        cb_data->node.port = 0;
    }else{
        if(evbuffer_remove(read_buf, (char*)&(cb_data->node.id), ID_BYTES) < 0 ||
                evbuffer_remove(read_buf, (char*)&(cb_data->node.IP), 4) < 0 ||
                evbuffer_remove(read_buf, (char*)&(cb_data->node.port), 2) < 0)
        {
            log_err("found? but error reading");
            net_connection_close(self->net, connection);
            cb_data->node.id = 0;
            cb_data->node.IP = 0;
            cb_data->node.port = 0;
        }
    }

    net_connection_close(self->net, connection);

    node_found(-1, 0, cb_data);
}

// TODO maybe close an free stuff
void node_remote_find_event(int connection, short type, void *arg)
{
    ///log_info("node_remote_find_event");
}

//
// node finding
//

//ask node n for successor of id
int node_find_successor_remote(struct node_self* self, struct node_info n,
        hash_type id, struct node_found_cb_data* cb_data)
{
    ///log_info("find_succ_remote");
    ///log_info("asking %08X @ %08X:%d for succ of %08X", n.id, n.IP, n.port, id);
    struct node_message msg;
    msg.from = self->self;
    msg.to   = n;
    msg.type = MSG_T_SUCC_REQ;
    msg.len  = ID_BYTES;
    msg.content = NULL;

    //log_info("built msg");
    int conn = node_connect_and_send_message(self, &msg, node_found_remote_cb,
            node_remote_find_event, (void*) cb_data, NODE_WAIT_TM_LONG);
    if (conn < 0){
        log_err("failed to create connection");
        return conn;
    }
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, conn);

    if (!write_buf){
        log_err("failed to get write buffer");
        return -1;
    }

    evbuffer_add(write_buf, (char*)&(id), ID_BYTES);
    return 0;
}

int node_find_successor(struct node_self* self, hash_type id, node_found_cb_t cb, void* found_cb_arg)
{
    ///log_info("looking for successor of %08X", id);
    ///log_info("my id is %08X",self->self.id);
    ///log_info("my succ's id is %08X",self->successor[0].id);

    struct node_found_cb_data *cb_data;
    if (node_id_compare(self->self.id, id) == 0){ // id is my id
        ///log_info("it's me");

        cb_data = malloc(sizeof(struct node_found_cb_data));
        cb_data->self         = self;
        cb_data->cb           = cb;
        cb_data->found_cb_arg = found_cb_arg;
        cb_data->node         = self->self;

        node_found(0, 0, cb_data);
    }
    else
        if(node_id_in_range(id, self->self.id, self->successor[0].id) ||
                node_id_compare(self->self.id, self->successor[0].id) == 0)
        { // id is between me and my successor
            ///log_info("it's my succ");

            cb_data = malloc(sizeof(struct node_found_cb_data));
            cb_data->self         = self;
            cb_data->cb           = cb;
            cb_data->found_cb_arg = found_cb_arg;
            cb_data->node         = self->successor[0];

            node_found(0, 0, cb_data);

        }else{ // need to ask another node to find it
            ///log_info("need to ask someone else");
            struct node_info n = node_closest_preceding_node(self, id); // node to ask

            cb_data = malloc(sizeof(struct node_found_cb_data));
            cb_data->self         = self;
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
    cb_data->self         = self;
    cb_data->cb           = cb;
    cb_data->found_cb_arg = found_cb_arg;


    //log_info("asking for pred");

    node_connect_and_send_message(self, &msg, node_found_remote_cb,
            node_remote_find_event, (void*) cb_data, NODE_WAIT_TM_LONG);
}

struct node_info node_closest_preceding_node(struct node_self* self, hash_type id)
{
    for (hash_type i = ID_BITS-1; i > 0; --i){
        struct node_info n = self->finger_table[i];
        if ((n.IP != 0 && n.port != 0 && n.id != 0) && node_id_in_range(n.id, self->self.id, id)){
            return (n);
        }
    }
    return (self->successor[0]);
}

//
// network Stabilization
//

//found successor's predecessor (stabilize part 2)
void node_stabilize_sp_found(struct node_info succ, void *arg){
    log_info("got succ's pred for stab");

    struct node_self* self = (struct node_self*) arg;

    if (succ.port == 0 && succ.IP == 0){ // successor doesn't know its predecessor
        //log_info("succ doesn't know its pred");
    }else
        log_info("my id is      : %08X", self->self.id);
        log_info("my pred is    : %08X", self->predecessor.id);
        log_info("current succ  : %08X", self->successor[0].id);
        log_info("potential succ: %08X", succ.id);
        // if (me < s->p < s) then update me->s
        if (node_id_compare(self->self.id, self->successor[0].id) == 0 ||
                node_id_in_range(succ.id, self->self.id + 1, self->successor[0].id)){
            self->successor[0] = succ;
        }
        log_info("my succ now is: %08X", self->successor[0].id);
    // notify s
    node_notify_node(self, self->successor[0]);
}

void node_network_stabalize(struct node_self* self)
{
    //log_info("stabilizing");
    node_get_predecessor_remote(self, self->successor[0], node_stabilize_sp_found, self);
}

// TODO
void node_notify_rep_event(int connection, short type, void *arg){
    struct node_self* self = (struct node_self*) arg;
    if (type & (BEV_EVENT_EOF|BEV_EVENT_ERROR|BEV_EVENT_TIMEOUT)) {
        net_connection_close(self->net, connection);
    }
    // TODO remove successor if conn error
}

void node_notify_node(struct node_self* self, struct node_info node)
{
    struct node_message msg;
    msg.from    = self->self;
    msg.to      = node;
    msg.type    = MSG_T_NOTIF;
    msg.len     = ID_BYTES + 2;
    msg.content = NULL;

    int conn = node_connect_and_send_message(self, &msg, NULL, node_notify_rep_event, (void*)self, NODE_WAIT_TM_DEFAULT);
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, conn);

    evbuffer_add(write_buf, (char*)&(self->self.id), ID_BYTES);
    evbuffer_add(write_buf, (char*)&(self->self.port), 2);
}

void node_notified(struct node_self* self, struct node_info node)
{
    log_info("got notified");
    if (self->has_pred){
        log_info("current pred is   %08X", self->predecessor.id);
    }else{
        log_info("current pred is   NONE");
    }
        log_info("potential pred is %08X", node.id);

    if (!self->has_pred ||
            node_id_in_range(node.id, self->predecessor.id, self->self.id) ||
            node_id_compare(self->predecessor.id, self->self.id) == 0
    ){
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

    // can't be my own finger
    if (node_id_compare(fua->self->self.id, node.id) != 0){
        fua->self->finger_table[fua->finger_num] = node;
    }else{
        struct node_info null_node;
        null_node.IP = 0;
        null_node.id = 0;
        null_node.port = 0;
        fua->self->finger_table[fua->finger_num] = null_node;
    }
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
    //log_info("fixing fingers");
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
    if (token[0] == 'Y'){
        // HOORAY
        //log_info("pred is not dead");
    }else{
        // couldn't reach pred
        self->has_pred = 0;
        memset(&(self->predecessor), 0, sizeof(struct node_info));
        //log_info("pred is dead");
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

void node_check_node(struct node_self* self, struct node_info node,
                        net_connection_data_cb_t reply_handler,
                        net_connection_event_cb_t event_handler, void* arg)
{
    struct node_message msg;
    msg.from    = self->self;
    msg.to      = node;
    msg.type    = MSG_T_ALIVE_REQ;
    msg.len     = 0;
    msg.content = NULL;

    node_connect_and_send_message(self, &msg, reply_handler,
            event_handler, arg, NODE_WAIT_TM_DEFAULT);
}

void node_check_successors(struct node_self* self)
{
    struct check_succ_arg arg;
    for (int i = 0; i < NUM_OF_SUCCS; ++i){
        if (self->successor[i].IP != 0){
            arg = malloc(sizeof(struct check_succ_arg));
            node_check_node(self, self->successor[i], node_check_succ_reply, node_check_succ_event, (void*) arg);
        }
    }
}


void node_check_predecessor(struct node_self* self)
{
    if (!self->has_pred){ return; }
    //log_info("checking predecessor");
    node_check_node(self, self->predecessor, node_check_predecessor_reply, node_check_predecessor_event, (void*) self);

}

//
// network I/O wrapper and node communication things
//

char* node_msg_type_str(int msg_type){
    switch (msg_type){
        case MSG_T_ALIVE_REQ:
            return "REQ_ALIVE";
        case MSG_T_NOTIF:
            return "REQ_NOTIFY";
        case MSG_T_PRED_REQ:
            return "REQ_PRED";
        case MSG_T_SUCC_REQ:
            return "REQ_SUCC";
        case MSG_T_ALIVE_REP:
            return "RESP_ALIVE";
        case MSG_T_PRED_REP:
            return "RESP_PRED";
        case MSG_T_SUCC_REP:
            return "RESP_SUCC";
        case MSG_T_NODE_MSG:
            return "NODE_MSG";
        case MSG_T_UNKNOWN:
            return "UNKNOWN";
        default:
            return "NONE";
    }
}

int node_send_message(struct node_self* self, struct node_message* msg, const int connection)
{
    //log_info("node_send_message type: %c", msg->type);
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    if (!write_buf){
        return -1; }

    //log_info("got write buf");

    int rc = 0;

    if (msg->content != NULL){
        rc = evbuffer_add_printf(write_buf, MSG_FMT_CONTENT, msg->type, msg->len, msg->content);
    }else{
        rc = evbuffer_add_printf(write_buf, MSG_FMT, msg->type, 0);
    }

    //log_info("added content to buf");

    if (rc < 0){
        return rc;
    }

// TODO should this function activate it?
    //log_info("activating connection");
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

    //log_info("got connection %d", connection);
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

    //log_info("calling send msg");
    int rc = node_send_message(self, msg, connection);
    if (rc == 0){
        return connection;
    }else{
        return rc;
    }

}


void node_successor_found_for_remote(struct node_info succ, void *data)
{
    //log_info("successor found for remote at %08X:%d", succ.IP, succ.port);
    struct incoming_handler_data* handler_data = (struct incoming_handler_data*) data;
    struct evbuffer* write_buf = net_connection_get_write_buffer(
            handler_data->self->net, handler_data->connection);

    evbuffer_add(write_buf, "Y", 1);
    evbuffer_add(write_buf, (char*)&(succ.id), ID_BYTES);
    evbuffer_add(write_buf, (char*)&(succ.IP), 4);
    evbuffer_add(write_buf, (char*)&(succ.port), 2);

    free(handler_data);
}


void handle_succ_request(int connection, void *arg)
{
    //log_info("handling succ req");

    hash_type r_id;
    struct node_self* self = (struct node_self*) arg;
    struct evbuffer* read_buf = net_connection_get_read_buffer(self->net, connection);

    if (evbuffer_remove(read_buf, (char*)&(r_id), ID_BYTES) < 0){
        log_err("error reading id");
        net_connection_close(self->net, connection);
    }

    struct incoming_handler_data *handler_data;
    handler_data = malloc(sizeof(struct incoming_handler_data));
    handler_data->self = self;
    handler_data->connection = connection;
    node_find_successor(self, r_id, node_successor_found_for_remote, handler_data);
}

// TODO
void handle_pred_request(int connection, void *arg)
{
    //log_info("handling pred req");
    struct node_self* self = (struct node_self*) arg;
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    if (self->has_pred){
        //log_info("pred is %08X@%08X:%04X", self->predecessor.id, self->predecessor.IP, self->predecessor.port);
        evbuffer_add(write_buf, "Y", 1);
        evbuffer_add(write_buf, (char*)&(self->predecessor.id), ID_BYTES);
        evbuffer_add(write_buf, (char*)&(self->predecessor.IP), 4);
        evbuffer_add(write_buf, (char*)&(self->predecessor.port), 2);

    }else{
        //log_info("No pred");
        evbuffer_add(write_buf, "N", 1);
    }
}

void handle_alive_request(int connection, void *arg)
{
    //log_info("handling alive req");
    struct node_self* self = (struct node_self*) arg;
    struct evbuffer* write_buf = net_connection_get_write_buffer(self->net, connection);
    evbuffer_add(write_buf, "Y\n", 6);
}

void handle_notif_request(int connection, void *arg)
{
    //log_info("handling notif req");

    struct node_info other;

    struct node_self* self = (struct node_self*) arg;
    struct evbuffer* read_buf = net_connection_get_read_buffer(self->net, connection);

    other.IP = net_connection_get_remote_address(self->net, connection);

    if ( !other.IP ||
            evbuffer_remove(read_buf, (char*)&(other.id), ID_BYTES) < 0 ||
            evbuffer_remove(read_buf, (char*)&(other.port), 2) < 0)
    {
        // error
        net_connection_close(self->net, connection);
    }


    node_notified(self, other);
    net_connection_close(self->net, connection);
}

void handle_node_message(int connection, void *arg)
{
    ///log_info("handle node msg called");
    struct node_msg_arg* msgarg = (struct node_msg_arg*) arg;
    struct node_self* self = msgarg->self;

    (self->msg_cb)(self, &msgarg->msg, connection, self->msg_cb_arg);
    free(msgarg);

    //log_info("handle node msg called");
}

int node_parse_message_header(struct node_message* msg, struct evbuffer* read_buf)
{

    // type of message
    if (evbuffer_remove(read_buf, &(msg->type), 1) == -1)
    {
        log_err("failed to read msg type");
        // error reading
        return -1;
    }

    // length of content or 0 if none
    char buf[LEN_STR_BYTES + 1] = { '\0' };
    if (evbuffer_remove(read_buf, buf, LEN_STR_BYTES) == -1)
    {
        log_err("failed to read msg length");
        // error reading
        return -1;
    }
    else
    {
        char *endptr;
        msg->len = (uint32_t) strtoul(buf, &endptr, 16);
    }
    return 0;
}

// incoming connection event e.g. error eof
void incoming_event_cb(int connection, short type, void *arg)
{
    //log_info("incoming event");
    struct node_self* self = (struct node_self*) arg;
    if (type & (BEV_ERROR|BEV_EVENT_EOF)){ // close and free connection on error or closed by remote
        net_connection_close(self->net, connection);
    }
}

// incoming connection event e.g. error eof after changing arg for NODE_MSG
void incoming_event_msg_cb(int connection, short type, void *arg)
{
    //log_info("incoming event (msg)");
    struct node_self* self = (struct node_self*) ( ( (struct node_msg_arg*)arg )->self );
    if (type & (BEV_ERROR|BEV_EVENT_EOF)){ // close and free connection on error or closed by remote
        //log_info("Error or EOF, closing connection");
        net_connection_close(self->net, connection);
    }
}

//initial read evt (reads header and sets body handler)

void incoming_read_cb(int connection, void *arg)
{
    //log_info("incomming read cb");
    struct node_self* self = (struct node_self*) arg;

    struct node_message msg;
    struct evbuffer* read_buf = net_connection_get_read_buffer(self->net, connection);

    struct node_msg_arg* msgarg = malloc(sizeof(struct node_msg_arg));

    if (node_parse_message_header(&msg, read_buf) < 0){
        log_err("error parsing incoming header");
        // error parsing msg
        net_connection_close(self->net, connection);
        return;
    }
    if (evbuffer_get_length(read_buf) < msg.len){

        //log_info("less data in buffer than msg length, setting handlers");
        switch (msg.type){

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
                msgarg = malloc(sizeof(struct node_msg_arg));
                msgarg->self = self;
                msgarg->msg = msg;
                net_connection_set_read_cb(self->net, connection, handle_node_message);
                net_connection_set_event_cb(self->net, connection, incoming_event_msg_cb);
                net_connection_set_cb_arg(self->net, connection, msgarg);
                break;

            case MSG_T_SUCC_REP:
            case MSG_T_PRED_REP:
            case MSG_T_ALIVE_REP:
            default:
                log_warn("unexpected message type recieved on icoming connection");
                break;
        }

        struct bufferevent* bufev = net_connection_get_bufev(self->net, connection);
        bufferevent_setwatermark(bufev, EV_READ, msg.len, 0);
    }
    else{
        //log_info("whole message already in buffer, calling handler");
        switch (msg.type){

            case MSG_T_SUCC_REQ:
                handle_succ_request(connection, (void*) self);
                break;

            case MSG_T_PRED_REQ:
                handle_pred_request(connection, (void*) self);
                break;

            case MSG_T_ALIVE_REQ:
                handle_alive_request(connection, (void*) self);
                break;

            case MSG_T_NOTIF:
                handle_notif_request(connection, (void*) self);
                break;

            case MSG_T_NODE_MSG:
                //log_info("sending node msg up");
                msgarg = malloc(sizeof(struct node_msg_arg));
                msgarg->self = self;
                msgarg->msg = msg;
                handle_node_message(connection, msgarg);
                break;

            case MSG_T_SUCC_REP:
            case MSG_T_PRED_REP:
            case MSG_T_ALIVE_REP:
            default:
                log_warn("unexpected message type recieved on icoming connection");
                break;
        }


    }
}

// called when listen socket accepts connection
void incoming_connection(int connection, short type, void *arg)
{

    //log_info("incoming connection cb");
    struct node_self* self = (struct node_self*) arg;
    net_connection_set_read_cb(self->net, connection, incoming_read_cb);
    net_connection_set_event_cb(self->net, connection, incoming_event_cb);
    net_connection_set_cb_arg(self->net, connection, (void*)self);
    net_connection_set_timeouts(self->net, connection, NODE_WAIT_TM_DEFAULT, NODE_WAIT_TM_DEFAULT);
    struct bufferevent* bufev = net_connection_get_bufev(self->net, connection);
    bufferevent_setwatermark(bufev, EV_READ, 1 + LEN_STR_BYTES, 0);
}
