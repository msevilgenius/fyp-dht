#ifndef NET_WRAPPER_H_INCLUDED
#define NET_WRAPPER_H_INCLUDED

#include "netio.h"

/**
 *  this wrapper is used to direct incoming messages to the appropriate layer
 */

int netw_init();

void netw_destroy();

struct net_server* netw_net_server_create(const uint16_t port);

int netw_handler_set_cb(int handler_id, net_connection_event_cb_t incoming_connection_cb);

int netw_handler_set_cb_arg(int handler_id, void* incoming_cb_arg);

int netw_register_handler(net_connection_event_cb_t incoming_connection_cb, void* incoming_cb_arg);

int netw_net_connection_create(struct net_server* srv, const uint32_t IP, const uint16_t port, int handler_id);


#endif // NET_WRAPPER_H_INCLUDED
