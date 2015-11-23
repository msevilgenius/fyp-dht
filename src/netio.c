#include "netio.h"
#include <stdlib.h>
#include <string.h>

struct net_server* net_create_server()
{
    struct net_server *srv
    struct sockaddr_in serv_addr; //socket address to bind to

    memset(&serv_addr, 0, sizeof(struct sockaddr_in));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(LISTEN_PORT);

    srv->base = event_base_new();
    srv->listener_evt = evconnlistener_new_bind(srv->base, listen_evt_cb, NULL,
            LEV_OPT_CLOSE_ON_FREE, -1,
            (struct sockaddr*)&serv_addr, sizeof(serv_addr));


    return srv;
}

static void listen_evt_cb(struct evconnlistener *listener, evutil_socket_t fd,
        struct sockaddr *addr, int socklen, void *arg)
{
    struct event_base* base = evconnlistener_get_base(listener);

}
