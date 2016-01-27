#include "netio.h"
#include <stdlib.h>
#include <string.h>


struct cb_args{
    bufferevent_data_cb cb_handler;
    void* cb_arg;
};

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
        evconnlistener_disable(srv->listener_evt);
        event_base_loopbreak(srv->base);
        evconnlistener_free(srv->listener_evt);
        event_base_free(srv->base);
        free(srv);
    }
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

    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(bev, srv->incoming_handler, NULL, NULL, srv->incoming_handler_arg);

    bufferevent_enable(bev, EV_READ|EV_WRITE);

}

int net_server_run(struct net_server* srv)
{

    event_base_loop(srv->base, 0);

    return 0;
}

int connect_to(struct net_server* srv, uint32_t IP, uint16_t port)
{
    struct sockaddr_in sin;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(IP);
    sin.sin_port = htons(port);
    // TODO save connection to outcons

    return -1;
}

int net_send_message(struct net_server* srv, char* message, int connection,
                     bufferevent_data_cb reply_handler, void* rh_arg)
{
    struct event_base* base = srv->base;
    struct bufferevent *bev;

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

    // TODO get connection instead from outconns

    bufferevent_setcb(bev, reply_handler, NULL, NULL, rh_arg);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    evbuffer_add_printf(bufferevent_get_output(bev), "%s", message);

    if (bufferevent_socket_connect(bev,
            (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        bufferevent_free(bev);
        return -1;
    }

    return 0;
}















