#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "netio.h"#include "logging.h"

struct net_connection{
    struct bufferevent* bev;
    struct sockaddr_in sin;
    net_connection_data_cb_t read_cb;
    net_connection_data_cb_t write_cb;
    net_connection_event_cb_t evt_cb;
    void* upper_cb_arg;
    struct net_conn_cb_arg *net_cb_arg;
};

struct net_server{
    struct event_base *base;
    struct evconnlistener *listener_evt;
    struct net_connection connections[MAX_OPEN_CONNECTIONS];
    pthread_mutex_t connections_lock;
    net_connection_event_cb_t incoming_handler;
    void* incoming_handler_arg;
};

struct net_conn_cb_arg{
    int conn;
    struct net_server* srv;
};

//
// helpers
//

int net_valid_connection_num(int connection)
{
    return !(connection < 0 || connection >= MAX_OPEN_CONNECTIONS);
}

// must use locks with this!!!
int net_empty_connection_slot(struct net_server* srv)
{
    int conn = -1;
    for(int i = 0; i < MAX_OPEN_CONNECTIONS; ++i){
        if (srv->connections[i].bev == NULL){
            conn = i;
            break;
        }
    }

    return conn;
}

//
// connection callbacks
//

void net_connection_read_cb(struct bufferevent *bev, void *ctx)
{
    struct net_server* srv = ((struct net_conn_cb_arg*) ctx)->srv;
    int conn = ((struct net_conn_cb_arg*) ctx)->conn;
    log_info("connection read ready %d", conn);
    if(net_valid_connection_num(conn)){
        struct net_connection* connection = &(srv->connections[conn]);
        if (connection->read_cb)
            (connection->read_cb)(conn, connection->upper_cb_arg);
    }
}

void net_connection_write_cb(struct bufferevent *bev, void *ctx)
{
    struct net_server* srv = ((struct net_conn_cb_arg*) ctx)->srv;
    int conn = ((struct net_conn_cb_arg*) ctx)->conn;
    log_info("connection write ready %d", conn);
    if(net_valid_connection_num(conn)){
        struct net_connection* connection = &(srv->connections[conn]);
        if (connection->write_cb)
            connection->write_cb(conn, connection->upper_cb_arg);
    }
}

void net_connection_event_cb(struct bufferevent *bev, short what, void *ctx)
{

    struct net_server* srv = ((struct net_conn_cb_arg*) ctx)->srv;
    int conn = ((struct net_conn_cb_arg*) ctx)->conn;
    log_info("event occurred on connection %d", conn);
    if(net_valid_connection_num(conn)){
        struct net_connection* connection = &(srv->connections[conn]);
        if (connection->evt_cb)
            connection->evt_cb(conn, what, connection->upper_cb_arg);
        // TODO close/cleanup on error?
    }
}

//
// server_ creation etc.
//

struct net_server* net_server_create(const uint16_t port, net_connection_event_cb_t incoming_connection_cb, void* incoming_cb_arg)
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

    srv->incoming_handler = incoming_connection_cb;
    srv->incoming_handler_arg = incoming_cb_arg;

    srv->listener_evt = evconnlistener_new_bind(srv->base, listen_evt_cb, (void*) srv,
                                LEV_OPT_CLOSE_ON_FREE, -1,
                                (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (!srv->listener_evt){ // error creating listener
        event_base_free(srv->base);
        free(srv);
        return NULL;
    }

    if (pthread_mutex_init(&(srv->connections_lock), NULL) != 0){
        evconnlistener_free(srv->listener_evt);
        event_base_free(srv->base);
        free(srv);
        return NULL;
    }

    return srv;
}

void net_server_set_incoming_cb_arg(struct net_server *srv, void* arg)
{
    srv->incoming_handler_arg = arg;
}

void net_server_destroy(struct net_server* srv)
{
    if(srv){
        for(int i = 0; i < MAX_OPEN_CONNECTIONS; ++i){
            net_connection_close(srv, i);
        }
        evconnlistener_disable(srv->listener_evt);
        event_base_loopbreak(srv->base);
        evconnlistener_free(srv->listener_evt);
        event_base_free(srv->base);
        pthread_mutex_destroy(&(srv->connections_lock));
        free(srv);
    }
}

struct event_base* net_get_base(struct net_server* srv)
{
    if (!srv) return NULL;
    return srv->base;
}

void listen_evt_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *addr, int socklen, void *arg)
{

    log_info("listen event");
    struct net_server *srv = (struct net_server *) arg;

    struct event_base* base = evconnlistener_get_base(listener);

    pthread_mutex_lock(&(srv->connections_lock));
    int conn = net_empty_connection_slot(srv);
    if (conn < 0){
        // TODO log errors etc.
        // connections are full
        pthread_mutex_unlock(&(srv->connections_lock));
        return;
    }

    log_info("creating connection %d", conn);
    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    srv->connections[conn].bev = bev;

    pthread_mutex_unlock(&(srv->connections_lock));

    if (!bev){
        return;
    }

    srv->connections[conn].net_cb_arg = malloc(sizeof(struct net_conn_cb_arg));
    if (!srv->connections[conn].net_cb_arg){
        net_connection_close(srv, conn);
        return;
    }
    srv->connections[conn].net_cb_arg->conn = conn;
    srv->connections[conn].net_cb_arg->srv = srv;

    bufferevent_setcb(bev, net_connection_read_cb, net_connection_write_cb, net_connection_event_cb, srv->connections[conn].net_cb_arg);

    log_info("calling handler %d", conn);
    srv->incoming_handler(conn, BEV_EVENT_CONNECTED, srv->incoming_handler_arg);

    bufferevent_enable(bev, EV_READ|EV_WRITE);

    log_info("incoming enabled %d", conn);
}

int net_server_run(struct net_server* srv)
{

    event_base_loop(srv->base, 0);

    return 0;
}

//
// connection management
//

int net_connection_create(struct net_server* srv, const uint32_t IP, const uint16_t port)
{
    if(!srv) return -1;
    struct event_base *base = srv->base;

    pthread_mutex_lock(&(srv->connections_lock));
    // find empty connection slot
    int conn = net_empty_connection_slot(srv);
    if (conn == -1){
        pthread_mutex_unlock(&(srv->connections_lock));
        return -1;
    }

    struct sockaddr_in *sin = &(srv->connections[conn].sin);

    srv->connections[conn].bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    struct bufferevent *bev = srv->connections[conn].bev;

    if (!bev){
        pthread_mutex_unlock(&(srv->connections_lock));
        return -1;
    }

    memset(sin, 0, sizeof(struct sockaddr_in));
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(IP);
    sin->sin_port = htons(port);


    srv->connections[conn].net_cb_arg = malloc(sizeof(struct net_conn_cb_arg));
    if (!srv->connections[conn].net_cb_arg){
        net_connection_close(srv, conn);
        pthread_mutex_unlock(&(srv->connections_lock));
        return -1;
    }
    srv->connections[conn].net_cb_arg->conn = conn;
    srv->connections[conn].net_cb_arg->srv = srv;
    bufferevent_setcb(bev, net_connection_read_cb, net_connection_write_cb, net_connection_event_cb, srv->connections[conn].net_cb_arg);

    pthread_mutex_unlock(&(srv->connections_lock));
    log_info("created connection %d", conn);

    return conn;
}

void net_connection_close(struct net_server* srv, const int conn)
{
    if (net_valid_connection_num(conn)){
        struct bufferevent *bev = srv->connections[conn].bev;

        if(bev) {
            bufferevent_free(bev);
        }
        if (srv->connections[conn].net_cb_arg){
            free(srv->connections[conn].net_cb_arg);
        }
        srv->connections[conn].net_cb_arg = NULL;
        srv->connections[conn].bev = NULL;
        memset(&(srv->connections[conn].sin), 0, sizeof(struct sockaddr));
    }
}

int net_connection_set_read_cb(struct net_server* srv, const int conn, net_connection_data_cb_t cb)
{
    if (net_valid_connection_num(conn)){
        srv->connections[conn].read_cb = cb;
        return 0;
    }
    return -1;
}

int net_connection_set_write_cb(struct net_server* srv, const int conn, net_connection_data_cb_t cb)
{
    if (net_valid_connection_num(conn)){
        srv->connections[conn].write_cb = cb;
        return 0;
    }
    return -1;
}

int net_connection_set_event_cb(struct net_server* srv, const int conn, net_connection_event_cb_t cb)
{
    if (net_valid_connection_num(conn)){
        srv->connections[conn].evt_cb = cb;
        return 0;
    }
    return -1;
}

int net_connection_set_cb_arg(struct net_server* srv, const int conn, void *cb_arg)
{
    if (net_valid_connection_num(conn)){
        srv->connections[conn].upper_cb_arg = cb_arg;
        return 0;
    }
    return -1;
}

void* net_connection_get_cb_arg(struct net_server* srv, const int conn)
{
    if (net_valid_connection_num(conn)){
        return srv->connections[conn].upper_cb_arg;
    }
    return NULL;
}

void net_connection_set_timeouts(struct net_server* srv, const int conn, const struct timeval* read_tm, const struct timeval* write_tm)
{
    if (!net_valid_connection_num(conn)){
        return; }
    bufferevent_set_timeouts(srv->connections[conn].bev, read_tm, write_tm);
}

int net_connection_activate(struct net_server* srv, const int conn)
{
    if (net_valid_connection_num(conn)){

        log_info("activating connection %d", conn);

        struct sockaddr_in *sin = &(srv->connections[conn].sin);
        struct bufferevent *bev = srv->connections[conn].bev;

        if (bufferevent_socket_connect(bev, (struct sockaddr *)sin, sizeof(struct sockaddr)) < 0) {
            net_connection_close(srv, conn);
            return -1;
        }
        bufferevent_enable(bev, EV_READ|EV_WRITE);
        return 0;
    }
    return -1;
}

struct evbuffer* net_connection_get_read_buffer(struct net_server* srv, const int conn)
{
    if (net_valid_connection_num(conn)){
        return bufferevent_get_input(srv->connections[conn].bev);
    }
    return NULL;
};

struct evbuffer* net_connection_get_write_buffer(struct net_server* srv, const int conn)
{
    if (net_valid_connection_num(conn)){
        return bufferevent_get_output(srv->connections[conn].bev);
    }
    return NULL;
};

uint32_t net_connection_get_remote_address(struct net_server* srv, const int conn)
{
    if (net_valid_connection_num(conn)){

        struct sockaddr addr;
        struct sockaddr_in* s = (struct sockaddr_in*)(&addr);
        socklen_t len = sizeof(addr);

        int fd = bufferevent_getfd(srv->connections[conn].bev);

        int rc = getpeername(fd , &addr , &len);
        return ntohl(s->sin_addr.s_addr);
    }
    return 0;
}
