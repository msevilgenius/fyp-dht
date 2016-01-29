#ifndef NETIO_H
#define NETIO_H

#include <event2/event.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#define MAX_OUTGOING_CONNS 64


struct net_server{
    struct event_base *base;
    struct evconnlistener *listener_evt;
    struct bufferevent* out_conns[MAX_OUTGOING_CONNS];
    bufferevent_data_cb incoming_handler;
    void* incoming_handler_arg;
};


struct net_server* net_server_create(uint16_t port, bufferevent_data_cb msg_handler, void* handler_cb_arg);

void net_server_destroy(struct net_server* srv);

int net_server_run(struct net_server* srv);

/*
 * initialise a connection to another node/server
 * returns a connection number to use with net_send_message or negative number on error
 */
int net_connect_to(struct net_server* srv, uint32_t IP, uint16_t port);

/*
 * send a message through a connection
 */
int net_send_message(struct net_server* srv, char* message, int connection,
                     bufferevent_data_cb reply_handler, void* rh_arg);

static void listen_evt_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *addr, int socklen, void *arg);




#endif // NETIO_H
