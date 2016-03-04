#ifndef PROTO_H
#define PROTO_H

#define MSG_FMT "%X\n%X\n%s\n%d\n"
#define MSG_FMT_CONTENT "%X\n%X\n%s\n%d\n%s"

/* msg body node data : "%X\n%X\n%X\n"

IDXXXXXX
IPXXXXXX
PORT

*/

/*
find_successor:
req: find successor id
resp: successor [of id] is s_id
  --s_id needs actual address not just hash id
*/

#define REQ_SUCCESSOR "GET_SUCC"
#define RESP_SUCCESSOR "GOT_SUCC"

/*
stabilize:
req: who is your predecessor?
resp: predecessor is id
*/

#define REQ_PREDECESSOR "GET_PRED"
#define RESP_PREDECESSOR "GOT_PRED"

/*
notify:
req: notify id
resp: [ok]
*/

#define REQ_NOTIFY "NOTIF"
#define RESP_NOTIFY "NOTIFD" // is this necessary ?

/*
check_predecessor:
req: are you there
resp: yes i am here
*/

#define REQ_ALIVE "ALIVE?"
#define RESP_ALIVE "ALIVE"

#define NODE_MSG "NODEMSG"

#define MES_END ("\n\n")

/*
########\n              from
########\n              to
REQ_XXXXXX\n            message type
NNNN\n                  content length
ASJDGKADBJOTJGEJB...    [content]
*/


#endif // PROTO_H
