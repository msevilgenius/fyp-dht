#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "net_wrapper.h"
#include "node.h"

struct node_self *node;
struct net_server* net;
int handle;
pthread_t inthr;

void read_in_msg(struct node_self* node, struct node_message* msg, int conn, void *arg)
{
    char* data = malloc(sizeof(char) * (msg->len + 1));
    struct evbuffer* rbuf = net_connection_get_read_buffer(net, conn);
    evbuffer_remove(rbuf, data, sizeof(data));
    data[msg->len] = '\0';
    printf("received:\n%s\n", data);
    free(data);
}


void read_incoming(int conn, void *arg)
{
    char msg[1024];
    struct evbuffer* rbuf = net_connection_get_read_buffer(net, conn);
    evbuffer_remove(rbuf, msg, 1023);
    msg[1023] = '\0';
    printf("%s\n", msg);
}

void event_incoming(int conn, short what, void *arg)
{
    if (what & (BEV_ERROR|BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)){
        net_connection_close(net, conn);
    }
}

void incoming(int conn, short what, void *arg)
{
    net_connection_set_read_cb(net, conn, read_incoming);
    net_connection_set_event_cb(net, conn, event_incoming);
    struct timeval tmo = {5,0};
    net_connection_set_timeouts(net, conn, &tmo, &tmo);

    printf("incoming accepted\n");
}

void out_conn_event(int connection, short type, void *arg)
{
    if (type & (BEV_ERROR|BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)){
        net_connection_close(net, connection);
    }
}

void node_found(struct node_info ninfo, void* arg)
{
    printf("found node\n");
    char *msg = (char*) arg;
    /*
    int conn = netw_net_connection_create(net, ninfo.IP, ninfo.port, handle);
    struct evbuffer* wbuf = net_connection_get_write_buffer(net, conn);
    evbuffer_add_printf(wbuf, "%s\n", msg);
    free(msg);
    net_connection_set_event_cb(net, conn, event_incoming);
    struct timeval tmo = {5,0};
    net_connection_set_timeouts(net, conn, &tmo, &tmo);
    net_connection_activate(net, conn);
    */
    struct timeval tmo = {5,0};
    node_connect_and_send_message(node, msg, NULL, out_conn_event, NULL, &tmo);


}

void *in_thread(void *arg)
{
    char name[256];
    while (1){
        printf("enter name\n");
        char *msg = malloc(sizeof(char) * 1024);
        while (!fgets(name, 256, stdin));
        printf("enter message\n");
        while (!fgets(msg, 1024, stdin));
        node_find_successor(node, get_id(name), node_found, (void*)msg);
    }
}

void net_joined(void* arg)
{
    printf("joined_cb\n");
    pthread_create(&inthr, NULL, in_thread, NULL);
}

int main(int argc, char **argv){

    char* endptr;
    uint16_t port = strtoul(argv[1], &endptr, 10);
    printf("%s\n%d\n", argv[1], port);


    node = node_create(port, argv[2]);
    if (node){
        printf("gotnode\n");
    }
    net = node_get_net(node);
    if (net){
        printf("gotnet\n");
    }
    node_set_node_msg_handler(node, read_in_msg, NULL);


    if (argc > 4){
        unsigned char buf[sizeof(struct in_addr)];
        int rc = inet_pton(AF_INET, argv[3], buf);
        uint32_t ip = 0;
        for(int i = 0; i < 4; i++){
            ip += (buf[i] << (8* (3 - i)));
        }
        uint16_t port = (uint16_t) strtoul(argv[4], &endptr, 10);
        struct node_info ninfo;
        ninfo.IP = ip;
        ninfo.port = port;
        printf("%08X:%d\n", ip, port);
        node_network_join(node, ninfo, net_joined, NULL);
    }else{
        node_network_create(node, net_joined, NULL);
    }


    pthread_kill(inthr, SIGKILL);



    return 0;
}