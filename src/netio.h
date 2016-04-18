#ifndef NETIO_H
#define NETIO_H

#include <stdlib.h>

#include <event2/event.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "libdhtnet.h"

#define MAX_OPEN_CONNECTIONS 256



int net_valid_connection_num(const int conn);

void listen_evt_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *addr, int socklen, void *arg);


#endif // NETIO_H
