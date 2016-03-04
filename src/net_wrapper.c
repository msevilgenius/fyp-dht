#include <pthread.h>
#include <string.h>
#include <event2/buffer.h>
#include "net_wrapper.h"

#define INIT_MAX_HANDLERS 8
#define HANDLER_NAME_LEN 16

struct netw_handler{
    net_connection_event_cb_t incoming_connection_cb;
    void* incoming_cb_arg;
};

static pthread_mutex_t handlers_lock;
static int netw_num_handlers = 0;
static int netw_max_handlers = INIT_MAX_HANDLERS;
static struct netw_handler *handlers;

void netw_incomming_cb(int connection, short type, void *arg)
{
    struct net_server* net = (struct net_server*)arg;
    size_t line_len;

    char* endptr;
    char* token;

    struct evbuffer *read_buf = net_connection_get_read_buffer(net, connection);
    token = evbuffer_readln(read_buf, &line_len, EVBUFFER_EOL_LF);
    int h_id = strtol(token, &endptr, 16);

    pthread_mutex_lock(&handlers_lock);
    if (h_id < 0 || h_id > netw_num_handlers){
        pthread_mutex_unlock(&handlers_lock);

        return;
    }
    struct netw_handler* h = &(handlers[h_id]);

    pthread_mutex_lock(&handlers_lock);

    h->incoming_connection_cb(connection, type, h->incoming_cb_arg);

    free(token);
}

int netw_init()
{
    pthread_mutex_init(&handlers_lock, NULL);
    handlers = malloc(sizeof(struct netw_handler) * netw_max_handlers);
    return (handlers ? 0 : -1);
}

void netw_destroy()
{
    pthread_mutex_lock(&handlers_lock);
    if (handlers){
        free(handlers);
    }
    pthread_mutex_unlock(&handlers_lock);
    pthread_mutex_destroy(&handlers_lock);
}

struct net_server* netw_net_server_create(const uint16_t port)
{
    struct net_server* srv = net_server_create(port, netw_incomming_cb, NULL);

    net_server_set_incoming_cb_arg(srv, srv);

    return srv;
}

int netw_handler_set_cb(int handler_id, net_connection_event_cb_t incoming_connection_cb)
{
    pthread_mutex_lock(&handlers_lock);
    if (handler_id < 0 || handler_id > netw_num_handlers){
        pthread_mutex_unlock(&handlers_lock);
        return -1; }

    struct netw_handler* h = &(handlers[handler_id]);
    h->incoming_connection_cb = incoming_connection_cb;

    pthread_mutex_unlock(&handlers_lock);
    return 0;
}

int netw_handler_set_cb_arg(int handler_id, void* incoming_cb_arg)
{
    pthread_mutex_lock(&handlers_lock);
    if (handler_id < 0 || handler_id > netw_num_handlers){
        pthread_mutex_unlock(&handlers_lock);
        return -1; }

    struct netw_handler* h = &(handlers[handler_id]);
    h->incoming_cb_arg = incoming_cb_arg;

    pthread_mutex_unlock(&handlers_lock);
    return 0;
}

int netw_register_handler(net_connection_event_cb_t incoming_connection_cb, void* incoming_cb_arg)
{
    pthread_mutex_lock(&handlers_lock);

    if (netw_num_handlers == netw_max_handlers){
        struct netw_handler* tmp_hand = malloc(sizeof(struct netw_handler) * netw_max_handlers * 2);
        memcpy(handlers, tmp_hand, sizeof(struct netw_handler) * netw_max_handlers);
        free(handlers);
        netw_max_handlers *= 2;
        handlers = tmp_hand;
    }

    int h_id = netw_num_handlers;
    netw_num_handlers++;
    struct netw_handler* h = &(handlers[netw_num_handlers]);
    h->incoming_connection_cb = incoming_connection_cb;
    h->incoming_cb_arg = incoming_cb_arg;

    pthread_mutex_unlock(&handlers_lock);
    return h_id;
}

int netw_net_connection_create(struct net_server* srv, const uint32_t IP, const uint16_t port, int handler_id)
{
    pthread_mutex_lock(&handlers_lock);
    if (handler_id < 0 || handler_id > netw_num_handlers){
        pthread_mutex_unlock(&handlers_lock);
        return -1;
    }
    pthread_mutex_unlock(&handlers_lock);
    int conn = net_connection_create(srv, IP, port);
    struct evbuffer* wbuf = net_connection_get_write_buffer(srv, conn);
    evbuffer_add_printf(wbuf, "%X\n", handler_id);

    return conn;
}
