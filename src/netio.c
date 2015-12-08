#include "netio.h"
#include <stdlib.h>
#include <string.h>

struct net_server* net_server_create()
{
    struct net_server *srv
    struct sockaddr_in serv_addr; //socket address to bind to

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(LISTEN_PORT);

    srv = malloc(sizeof(net_server));

    if (!srv){
        // malloc failed
        return NULL;
    }

    srv->base = event_base_new();
    srv->listener_evt = evconnlistener_new_bind(srv->base, listen_evt_cb, NULL,
            LEV_OPT_CLOSE_ON_FREE, -1,
            (struct sockaddr*)&serv_addr, sizeof(serv_addr));


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
static void read_incoming_cb(struct bufferevent *bev, void *ctx)
{
    struct evbuffer *input = bufferevent_get_input(bev);
}

static void listen_evt_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *addr, int socklen, void *arg)
{
    struct event_base* base = evconnlistener_get_base(listener);

    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(bev, read_incoming_cb, NULL, echo_event_cb, NULL);

    bufferevent_enable(bev, EV_READ|EV_WRITE);

}

int net_server_run(struct net_server* srv)
{

    event_base_loop(srv->base, 0);

    return 0;
}

int connect_to(struct net_server* srv, uint32_t IP, uint16_t port)
{

}

int net_send_message(struct net_server* srv, char* message, uint32_t IP, uint16_t port,
                     bufferevent_data_cb reply_handler, void* rh_arg)
{
    struct event_base base* = srv->base;
    struct sockaddr_in sin;
    struct bufferevent *bev;

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(IP);
    sin.sin_port = htons(port);

    bufferevent_setcb(bev, reply_handler, NULL, eventcb, rh_arg);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    evbuffer_add_printf(bufferevent_get_output(bev), "%s", message);

    if (bufferevent_socket_connect(bev,
            (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        bufferevent_free(bev);
        return -1;
    }

    return 0;
}















