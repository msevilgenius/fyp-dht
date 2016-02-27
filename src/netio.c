#include "netio.h"
#include <stdlib.h>
#include <string.h>


struct net_connection{
    struct bufferevent* bev;
    struct sockaddr_in sin;
};

struct net_server{
    struct event_base *base;
    struct evconnlistener *listener_evt;
    struct net_connection connections[MAX_OUTGOING_CONNS];
    bufferevent_data_cb incoming_handler;
    void* incoming_handler_arg;
};

struct net_cb_args{
    bufferevent_data_cb cb_handler;
    void* cb_arg;
};

//
// server_ creation etc.
//

struct net_server* net_server_create(uint16_t port, bufferevent_data_cb msg_handler, void* handler_cb_arg)
{
    struct net_server *srv;
    struct sockaddr_in serv_addr; //socket address to bind to

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    srv = malloc(sizeof(struct net_server));

    if (!srv){ // malloc failed
        return NULL;
    }

    srv->base = event_base_new();
    if (!srv->base){ // error creating base
        free(srv);
        return NULL;
    }

    srv->incoming_handler     = msg_handler;
    srv->incoming_handler_arg = handler_cb_arg;

    srv->listener_evt = evconnlistener_new_bind(srv->base, listen_evt_cb, (void*) srv,
                                LEV_OPT_CLOSE_ON_FREE, -1,
                                (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (!srv->listener_evt){ // error creating listener
        event_base_free(srv->base);
        free(srv);
        return NULL;
    }

    return srv;
}

void net_server_destroy(struct net_server* srv)
{
    if(srv){
        for(int i = 0; i < MAX_OUTGOING_CONNS; ++i){
            net_close_connection(srv, i);
        }
        evconnlistener_disable(srv->listener_evt);
        event_base_loopbreak(srv->base);
        evconnlistener_free(srv->listener_evt);
        event_base_free(srv->base);
        free(srv);
    }
}

struct event_base* net_get_base(struct net_server* srv)
{
    return srv->base;
}

/*
 * callback for reading message from incoming connection
 */
static void read_incoming_cb(struct bufferevent *bev, void *arg)
{
    struct node_self* self_node = (struct node_self*) arg;
    struct evbuffer *input = bufferevent_get_input(bev);
}

static void listen_evt_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *addr, int socklen, void *arg)
{
    struct net_server *srv = (struct net_server *) arg;

    struct event_base* base = evconnlistener_get_base(listener);

    int conn = net_empty_connection_slot(srv);
    if (conn < 0){
        // TODO
        // connections are full
        return;
    }

    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

    srv->connections[conn].bev = bev;

    bufferevent_setcb(bev, srv->incoming_handler, NULL, NULL, srv->incoming_handler_arg);

    bufferevent_enable(bev, EV_READ|EV_WRITE);

}

int net_server_run(struct net_server* srv)
{

    event_base_loop(srv->base, 0);

    return 0;
}

//
// helpers
//

int net_valid_connection_num(int connection)
{
    return (connection < 0 || connection >= MAX_OUTGOING_CONNS);
}

// must use locks with this!!!
int net_empty_connection_slot(struct net_server* srv)
{
    int conn = -1;
    for(int i = 0; i < MAX_OUTGOING_CONNS; ++i){
        if (srv->connections[i].bev == NULL){
            conn = i;
            break;
        }
    }

    return conn;
}

//
// connection management
//

int net_create_connection(struct net_server* srv, uint32_t IP, uint16_t port)
{
    struct event_base *base = srv->base;
    struct bufferevent *bev = srv->connections[connection].bev;
    struct sockaddr *sin = &(srv->connections[connection].sin);

    // find empty connection slot
    // lock srv->conns
    int conn = net_empty_connection_slot(srv);
    if (conn == -1){
        // unlock srv->conns
        return -1;
    }

    memset(&(srv->connections[conn].sin), 0, sizeof(srv->connections[conn].sin));
    srv->connections[conn].sin.sin_family = AF_INET;
    srv->connections[conn].sin.sin_addr.s_addr = htonl(IP);
    srv->connections[conn].sin.sin_port = htons(port);


    struct event_base* base = srv->base;

    srv->connections[conn].bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

    // unlock srv->conns
    return conn;
}

void net_close_connection(struct net_server* srv, int connection)
{
    if (net_valid_connection_num(connection >= MAX_OUTGOING_CONNS)){
        struct bufferevent *bev = srv->connections[connection].bev;
        bufferevent_free(bev);
        srv->connections[connection].bev = NULL;
        memset(srv->connections[connection].sin, 0, sizeof(struct sockaddr_in));
    }
}

int net_connection_set_handlers(struct net_server* srv, int connection, read_ready, timeout, close)
{
    if (net_valid_connection_num(connection >= MAX_OUTGOING_CONNS)){
        struct bufferevent *bev = srv->connections[connection].bev;
        bufferevent_setcb(bev, ???? );
        return 0;
    }
    return -1;
}

int net_connection_activate(struct net_server* srv, int connection)
{
    if (net_valid_connection_num(connection >= MAX_OUTGOING_CONNS)){
        struct bufferevent *bev = srv->connections[connection].bev;

        if (bufferevent_socket_connect(bev, sin, sizeof(sin)) < 0) {
            net_close_connection(connection);
            // TODO do fail cb
            return -1;
        }
    }else{
        return -1;
    }
}

struct evbuffer* net_connection_get_read_buffer(struct net_server* srv, int connection)
{
    if (net_valid_connection_num(connection >= MAX_OUTGOING_CONNS)){
        return bufferevent_get_input(srv->connections[connection].bev);
    }
    return NULL;
};

struct evbuffer* net_connection_get_write_buffer(struct net_server* srv, int connection)
{
    if (net_valid_connection_num(connection >= MAX_OUTGOING_CONNS)){
        return bufferevent_get_output(srv->connections[connection].bev);
    }
    return NULL;
};














