#ifndef NETIO_H
#define NETIO_H

#include <stdlib.h>

#include <event2/event.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#define MAX_OUTGOING_CONNS 64

struct net_connection;

struct net_server;


struct net_server* net_server_create(uint16_t port, bufferevent_data_cb msg_handler, void* handler_cb_arg);

void net_server_destroy(struct net_server* srv);

int net_server_run(struct net_server* srv);

struct event_base* net_get_base(struct net_server* srv);

int net_valid_connection_num(int connection);

/*
 * initialise a connection to another node/server
 * returns a connection number to use with net_connection functions or negative number on error
 */
int net_connection_create(struct net_server* srv, uint32_t IP, uint16_t port);

/*
 * close a connection to another node/server
 */
int net_connection_close(struct net_server* srv, int connection);

/*
 * set handlers for read available, connection timeout, disconnection?
 */
int net_connection_set_handlers(struct net_server* srv, int connection, read_ready, timeout, close);

/*
 * actually open connection and begin I/O
 * for outgoing connections it is best to add message to write buffer then activating
 */
int net_connection_activate(struct net_server* srv, int connection);

/*
 * gets the connection's read buffer (contains data sent to this connection from remote)
 */
struct evbuffer* net_connection_get_read_buffer(struct net_server* srv, int connection);

/*
 * gets the connection's read buffer (contains data sent to this connection from remote)
 */
struct evbuffer* net_connection_get_write_buffer(struct net_server* srv, int connection);


static void listen_evt_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *addr, int socklen, void *arg);




#endif // NETIO_H
