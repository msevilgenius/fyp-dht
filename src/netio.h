#ifndef NETIO_H
#define NETIO_H

#include <event2/event.h>
#include <event2/util.h>
#include <event2/listener.h>


struct net_server{
    struct event_base *base;
    struct evconnlistener *listener_evt;
};


struct net_server* net_create_server();

static void listen_evt_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *addr, int socklen, void *arg);




#endif // NETIO_H
