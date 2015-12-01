#ifndef NETIO_H
#define NETIO_H

#include <event2/event.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/buffer.h>


struct net_server{
    struct event_base *base;
    struct evconnlistener *listener_evt;
};


struct net_server* net_server_create();

void net_server_destroy(struct net_server* srv);

int net_server_run(struct net_server* srv);

int net_send_message(struct net_server* srv, char** message, uint32_t IP, uint16_t port);

static void listen_evt_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *addr, int socklen, void *arg);




#endif // NETIO_H
