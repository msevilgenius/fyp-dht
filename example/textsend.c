#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "net_wrapper.h"
#include "node.h"
#include "proto.h"

struct node_self *node;
struct net_server* net;
int handle;
pthread_t inthr;

short running;

void read_in_msg(struct node_self* node, struct node_message* msg, int conn, void *arg)
{
    //printf("msg->len: %d\n", msg->len);
    struct evbuffer* rbuf = net_connection_get_read_buffer(net, conn);
    //printf("buffer len: %lu\n", evbuffer_get_length(rbuf));
    unsigned char* data = evbuffer_pullup(rbuf, -1);
    data[msg->len] = '\0';
    printf("received:\n%s\n", data);
    evbuffer_drain(rbuf, -1);
}


void out_conn_event(int connection, short type, void *arg)
{
    if (type & (BEV_ERROR|BEV_EVENT_EOF|BEV_EVENT_TIMEOUT)){
        net_connection_close(net, connection);
    }
}

void found_node(struct node_info ninfo, void* arg, short hops)
{
    //printf("found node?\n");
    printf("found node in %d hops\n", hops);
    char *msg = (char*) arg;
    //printf("sending [%lu] %s\n", strlen(msg), msg);
    struct node_message nmsg;
    nmsg.to = ninfo;
    nmsg.content = msg;
    nmsg.len = strlen(msg);
    nmsg.type = MSG_T_NODE_MSG;
    struct timeval tmo = {5,0};
    node_connect_and_send_message(node, &nmsg, NULL, out_conn_event, NULL, &tmo);
    free(msg);


}

void *in_thread(void *arg)
{
    char name[256];
    while (running){
        printf("enter name\n");
        char *msg = malloc(sizeof(char) * 1024);
        while (!fgets(name, 256, stdin));
        printf("enter message\n");
        while (!fgets(msg, 1024, stdin));
        //printf("sending %s\n", msg);
        node_find_successor(node, get_id(name), found_node, (void*)msg);
    }
    pthread_exit(NULL);
}

void net_joined(void* arg)
{
    printf("Joined Network\n");
    running = 1;
    pthread_create(&inthr, NULL, in_thread, NULL);
}

int main(int argc, char **argv){

    char* endptr;
    uint16_t port = (uint16_t)strtoul(argv[1], &endptr, 10);


    node = node_create(port, argv[2]);
    if (!node){
        return -1;
    }
    net = node_get_net(node);
    if (!net){
        return -1;
    }
    node_set_node_msg_handler(node, read_in_msg, NULL);


    if (argc > 4){
        unsigned char buf[sizeof(struct in_addr)];
        inet_pton(AF_INET, argv[3], buf);
        uint32_t ip = 0;
        for(int i = 0; i < 4; i++){
            ip += (buf[i] << (8* (3 - i)));
        }
        uint16_t port = (uint16_t) strtoul(argv[4], &endptr, 10);
        struct node_info ninfo;
        ninfo.IP = ip;
        ninfo.port = port;
        node_network_join(node, ninfo, net_joined, NULL);
    }else{
        node_network_create(node, net_joined, NULL);
    }


    //pthread_kill(inthr, SIGKILL);

    running = 0;
    pthread_join(inthr, NULL);


    return 0;
}
