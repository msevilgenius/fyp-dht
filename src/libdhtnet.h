#ifndef LIBDHTNET_H
#define LIBDHTNET_H

#include <stdlib.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

struct net_connection;

struct net_server;


typedef void (*net_connection_data_cb_t)(int connection, void *arg);
typedef void (*net_connection_event_cb_t)(int connection, short type, void *arg);



struct net_server* net_server_create(const uint16_t port, net_connection_event_cb_t incoming_connection_cb, void* incoming_cb_arg);

void net_server_set_incoming_cb_arg(struct net_server *srv, void* arg);

void net_server_stop(struct net_server* srv);

void net_server_destroy(struct net_server* srv);

int net_server_run(struct net_server* srv);

struct event_base* net_get_base(struct net_server* srv);

/*
 * initialise a connection to another node/server (doesn't actually connect)
 * returns a connection number to use with net_connection functions or negative number on error
 */
int net_connection_create(struct net_server* srv, const uint32_t IP, const uint16_t port);

/*
 * close a connection to another node/server
 */
void net_connection_close(struct net_server* srv, const int conn);

/*
 * set handlers for read available, connection timeout, disconnection
 */
int net_connection_set_read_cb(struct net_server* srv, const int conn, net_connection_data_cb_t cb);

int net_connection_set_write_cb(struct net_server* srv, const int conn, net_connection_data_cb_t cb);

int net_connection_set_event_cb(struct net_server* srv, const int conn, net_connection_event_cb_t cb);

int net_connection_set_cb_arg(struct net_server* srv, const int conn, void *cb_arg);

void* net_connection_get_cb_arg(struct net_server* srv, const int conn);

void net_connection_set_timeouts(struct net_server* srv, const int conn, const struct timeval* read_tm, const struct timeval* write_tm);

struct bufferevent* net_connection_get_bufev(struct net_server* srv, const int conn);
/*
 * actually open connection and begin I/O
 * for outgoing connections it is best to add message to write buffer then activating
 */
int net_connection_activate(struct net_server* srv, const int conn);

/*
 * gets the connection's read buffer (contains data sent to this connection from remote)
 */
struct evbuffer* net_connection_get_read_buffer(struct net_server* srv, const int conn);

/*
 * gets the connection's read buffer (contains data sent to this connection from remote)
 */
struct evbuffer* net_connection_get_write_buffer(struct net_server* srv, const int conn);

uint32_t net_connection_get_remote_address(struct net_server* srv, const int conn);



#endif // LIBDHTNET_H
